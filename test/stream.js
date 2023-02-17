const { Readable, Writable } = require('stream');
const { randomBytes, createHash } = require('crypto');
const child_process = require('child_process');
const path = require('path');
const { expect } = require('chai');
const {
    Disruptor,
    DisruptorReadStream,
    DisruptorWriteStream
}  = require('..');


class RandomStream extends Readable {
    constructor(size) {
        super();
        this.size = size;
        this.hash = createHash('sha256');
        this.reading = false;
    }

    _read() {
        if (this.reading) {
            return;
        }
        this.reading = true;

        if (this.size === 0) {
            return this.push(null);
        }

        randomBytes(this.size, (err, buf) => {
            this.reading = false;
            if (err) {
                return this.emit('error', err);
            }
            this.size -= buf.length;
            this.hash.update(buf);
            if (this.size === 0) {
                this.digest = this.hash.digest('hex');
            }
            if (this.push(buf)) {
                setImmediate(() => this._read());
            }
        });
    }
}

describe('stream functionality', function () {
    this.timeout(5 * 60 * 1000);

    let disruptors, streams;

    beforeEach(function () {
        disruptors = [new Disruptor('/test', 1000, 1, 1, 0, true, false)];
        streams = [];
    });

    afterEach(function (cb) {
        function release() {
            for (const d of disruptors) {
                d.release();
            }
            cb();
        }

        if (streams.length === 0) {
            return release();
        }

        let count = 0;

        function closed() {
            if (++count === streams.length) {
                release();
            }
        }

        for (const s of streams) {
            if (s.destroyed) {
                closed();
            } else {
                s.on('close', closed);
                s.destroy();
            }
        }
    });

    it('should send data from writer to reader', function (done) {
        const rs = new DisruptorReadStream(disruptors[0]);
        const ws = new DisruptorWriteStream(disruptors[0]);

        streams.push(rs, ws);

        const bufs = [];

        rs.on('readable', function () {
            while (true) {
                const buf = this.read();
                if (!buf) {
                    break;
                }
                bufs.push(buf);
            }
        });

        rs.on('end', function () {
            expect(Buffer.concat(bufs).toString()).to.equal('hello');
            done();
        });

        ws.end('hello');
    });

    it('should pipe data', function (done) {
        const rs = new DisruptorReadStream(disruptors[0]);
        const ws = new DisruptorWriteStream(disruptors[0]);
        const rngs = new RandomStream(1024 * 1024);

        streams.push(rs, ws);

        rngs.pipe(ws);

        rs.pipe(new class extends Writable {
            constructor(options) {
                super(options);
                this.hash = createHash('sha256');
                this.count = 0;
            }
        }({
            autoDestroy: true, // for Node 12

            /*construct(cb) {
                this.hash = createHash('sha256');
                cb();
            },*/
            
            write(chunk, encoding, cb) {
                this.hash.update(chunk);
                this.count += chunk.length;
                cb();
            },

            final(cb) {
                expect(this.count).to.equal(1024 * 1024);
                expect(this.hash.digest('hex')).to.equal(rngs.digest);
                cb();
            }
        }).on('close', done));
    });

    it('should propagate writer error', function (done) {
        const rs = new DisruptorReadStream(disruptors[0]);
        const ws = new DisruptorWriteStream(disruptors[0]);

        streams.push(rs, ws);

        rs.on('error', function (err) {
            expect(err.message).to.equal('writer errored');
            done();
        });

        rs.on('readable', function () {
        });

        ws.on('error', function (err) {
            expect(err.message).to.equal('foo');
        });

        ws.destroy(new Error('foo'));
    });

    it("should error if element size isn't 1", function () {
        const d2 = new Disruptor('/test2', 1000, 2, 1, 0, true, false);

        expect(function () {
            new DisruptorReadStream(d2);
        }).to.throw('element size must be 1');

        expect(function () {
            new DisruptorWriteStream(d2);
        }).to.throw('element size must be 1');

        d2.release();
    });

    it("should error if spin isn't false", function () {
        const d2 = new Disruptor('/test2', 1000, 1, 1, 0, true, true);

        expect(function () {
            new DisruptorReadStream(d2);
        }).to.throw('spin must be false');

        expect(function () {
            new DisruptorWriteStream(d2);
        }).to.throw('spin must be false');

        d2.release();
    });

    it('should support multiple readers', function (done) {
        disruptors[0].release();
        disruptors = [];

        const num_readers = 1000;

        let count = 0;
        function check() {
            if (++count === num_readers) {
                done();
            }
        }

        const rngs = new RandomStream(1024 * 1024);

        for (let i = 0; i < num_readers; ++i) {
            const d = new Disruptor('/test', 10000, 1, num_readers, i, i === 0, false);
            disruptors.push(d);
            const rs = new DisruptorReadStream(d);
            streams.push(rs);

            rs.pipe(new class extends Writable {
                constructor(options)
                {
                    super(options);
                    this.hash = createHash('sha256');
                }
            }({
                autoDestroy: true, // for Node 12

                /*construct(cb) {
                    this.hash = createHash('sha256');
                    cb();
                },*/
                
                write(chunk, encoding, cb) {
                    this.hash.update(chunk);
                    cb();
                },

                final(cb) {
                    expect(this.hash.digest('hex')).to.equal(rngs.digest);
                    cb();
                }
            }).on('close', check));
        }

        const d = new Disruptor('/test', 10000, 1, num_readers, 0, false, false);
        disruptors.push(d);
        const ws = new DisruptorWriteStream(d);
        streams.push(ws);
        rngs.pipe(ws);
    });

    it('should destroy while reading', function (done) {
        const rs = new DisruptorReadStream(disruptors[0]);
        streams.push(rs);
        let the_err;
        rs.on('close', () => {
            expect(the_err.message).to.equal('foo');
            done();
        });
        rs.on('error', err => {
            the_err = err;
        });
        rs._read();
        rs.destroy(new Error('foo'));
    });

    it('should destroy while writing', function (done) {
        const ws = new DisruptorWriteStream(disruptors[0]);
        streams.push(ws);
        let the_err;
        ws.on('close', () => {
            expect(the_err.message).to.equal('foo');
            done();
        });
        ws.on('error', err => {
            the_err = err;
        });
        ws.write('bar');
        ws.destroy(new Error('foo'));
    });

    it('should not read once destroyed', function (done) {
        const rs = new DisruptorReadStream(disruptors[0]);
        streams.push(rs);
        rs.on('data', () => {
            done(new Error('unexpected data'));
        });
        rs.on('close', async function () {
            await this._read();
            done();
        });
        rs.destroy();
    });

    it('should not write once destroyed', function (done) {
        const ws = new DisruptorWriteStream(disruptors[0]);
        streams.push(ws);
        ws.on('close', async function () {
            await this._write();
            done();
        });
        ws.destroy();
    });

    it('should catch disruptor errors while reading', function (done) {
        const rs = new DisruptorReadStream(disruptors[0]);
        streams.push(rs);
        rs.on('error', err => {
            expect(err.message).to.equal('foobar');
            done();
        });
        disruptors[0].consumeNew = () => {
            throw new Error('foobar');
        };
        rs.read();
    });

    it('should catch disruptor errors while writing', function (done) {
        const ws = new DisruptorWriteStream(disruptors[0]);
        streams.push(ws);
        ws.on('error', err => {
            expect(err.message).to.equal('foobar');
            done();
        });
        disruptors[0].produceClaimAvail = () => {
            throw new Error('foobar');
        };
        ws.write('test');
    });

    it('should catch disruptor errors while committing', function (done) {
        const ws = new DisruptorWriteStream(disruptors[0]);
        streams.push(ws);
        ws.on('error', err => {
            expect(err.message).to.equal('foobar');
            done();
        });
        disruptors[0].produceCommit = () => {
            throw new Error('foobar');
        };
        ws.write('test');
    });

    it('should retry commit', function (done) {
        const rs = new DisruptorReadStream(disruptors[0]);
        const ws = new DisruptorWriteStream(disruptors[0]);

        streams.push(rs, ws);

        const bufs = [];

        rs.on('readable', function () {
            while (true) {
                const buf = this.read();
                if (!buf) {
                    break;
                }
                bufs.push(buf);
            }
        });

        let count = 0;
        const produceCommit = disruptors[0].produceCommit;

        disruptors[0].produceCommit = function () {
            if (++count === 1) {
                return false;
            }

            return produceCommit.apply(this, arguments);
        };

        let ended = false;
        let finished = false;

        rs.on('end', function () {
            expect(Buffer.concat(bufs).toString()).to.equal('hello');
            ended = true;
            if (finished) {
                done();
            }
        });

        ws.on('finish', function () {
            finished = true;
            expect(count).to.equal(2);
            if (ended) {
                done();
            }
        });

        ws.end('hello');
    });

    it('should not commit when destroyed', function (done) {
        const ws = new DisruptorWriteStream(disruptors[0]);
        streams.push(ws);

        let produce_commit_called = false;
        disruptors[0].produceCommit = function () {
            produce_commit_called = true;
        };

        let the_err;
        ws.on('error', function (err) {
            expect(err.message).to.equal('foo');
            setTimeout(() => {
                expect(produce_commit_called).to.be.false;
                done();
            }, 1000);
        });

        const commit = ws._commit;
        ws._commit = function () {
            this.destroy(new Error('foo'));
            return commit.apply(this, arguments);
        };

        ws.write('hello');
    });

    it('should handle destroy while committing', function (done) {
        const ws = new DisruptorWriteStream(disruptors[0]);
        streams.push(ws);

        ws.on('close', done);

        disruptors[0].produceCommit = function () {
            ws.destroy();
        };

        ws.write('hello');
    });

    it('should error if all consumers ignore data', function (done) {
        disruptors[0].release();
        disruptors = [];

        const num_readers = 1000;

        let count = 0;

        function readable() {
            expect(this.read().toString()).to.equal('A');
            this.on('close', function () {
                this.disruptor.release(true);
                if (++count === num_readers) {
                    ws.once('error', function (err) {
                        expect(err.message).to.equal('no consumers');
                        done();
                    });
                    ws.write('B');
                }
            });
            this.destroy();
        }

        for (let i = 0; i < num_readers; ++i) {
            const d = new Disruptor('/test', 10000, 1, num_readers, i, i === 0, false);
            new DisruptorReadStream(d).once('readable', readable);
        }

        const d = new Disruptor('/test', 10000, 1, num_readers, 0, false, false);
        disruptors.push(d);
        const ws = new DisruptorWriteStream(d);
        streams.push(ws);
        ws.write('A');
    });

    it('should retry writing if Disruptor is full', function (done) {
        const rs = new DisruptorReadStream(disruptors[0], { highWaterMark: 1000 });
        const ws = new DisruptorWriteStream(disruptors[0]);

        streams.push(rs, ws);

        rs.once('readable', function () {
            ws.write('A');
            setTimeout(() => {
                this.once('readable', function () {
                    expect(this.read().length).to.equal(1000);
                    this.once('readable', function () {
                        expect(this.read().toString()).to.equal('A');
                        done();
                    });
                });
                expect(this.read().length).to.equal(1000);

            }, 500);
        });

        // Fill up Disruptor and read stream buffer
        ws.write(Buffer.alloc(2000));
    });

    it("shouldn't read if already reading", function (done) {
        const rs = new DisruptorReadStream(disruptors[0]);
        streams.push(rs);

        let the_resolve = null;

        disruptors[0].consumeNew = function () {
            return new Promise((resolve, reject) => {
                the_resolve = resolve;
            });
        };

        rs.read();
        expect(the_resolve).not.to.be.null;
        rs.push('A');
        setTimeout(() => {
            the_resolve({ bufs: [] });
            done();
        }, 500);
    });

    it('should handle multiple consume buffers (#28)', function (done) {
        const d = new Disruptor('/stream', 5000, 1, 1, 0, true, false);
        disruptors.push(d);

        const rs = new DisruptorReadStream(d);
        streams.push(rs);

        let msg = '';
        let msgCount = 0;
        rs.on('data', (chunk) => {
            msg += chunk.toString('utf8');
            if (msg.charCodeAt(msg.length - 1) === 0) {
                const allData = msg.substring(0, msg.length - 1);
                allData.split("\0").map(message => {
                    const data = JSON.parse(message);
                    expect(data).to.eql({
                        test: 'Text',
                        data: msgCount++
                    });
                });
                msg = '';
            }
        });

        rs.on('end', () => {
            expect(msgCount).to.equal(1000);
            done();
        });

        child_process.fork(path.join(__dirname, 'producer.js'));
    });
});
