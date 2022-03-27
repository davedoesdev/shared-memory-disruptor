const { promisify } = require('util');
const { Readable, Writable } = require('stream');
const Disruptor = require('bindings')('disruptor.node').Disruptor;

const status_eof = 1;
const status_error = 2;

function check(cb, r, arg1, arg2)
{
    if (r !== undefined)
    {
        process.nextTick(cb, null, r, arg1, arg2);
    }
}

class Disruptor2 extends Disruptor
{
    constructor(...args)
    {
        super(...args);

        this._consumeNewAsync = promisify(cb => {
            this._consumeNew((err, bufs, start) => {
                cb(err, { bufs, start });
            });
        });

        this._produceClaimAsync = promisify(cb => {
            this._produceClaim((err, buf, claimStart, claimEnd) => {
                cb(err, { buf, claimStart, claimEnd });
            });
        });

        this._produceClaimManyAsync = promisify((n, cb) => {
            this._produceClaimMany(n, (err, bufs, claimStart, claimEnd) => {
                cb(err, { bufs, claimStart, claimEnd });
            });
        });

        this._produceClaimAvailAsync = promisify((max, cb) => {
            this._produceClaimAvail(max, (err, bufs, claimStart, claimEnd) => {
                cb(err, { bufs, claimStart, claimEnd });
            });
        });

        this._produceCommitAsync = promisify(function (claimStart, claimEnd, cb) {
            if (arguments.length >= 2) {
                return this._produceCommit(claimStart, claimEnd, cb);
            }
            this._produceCommit(claimStart);
        });
    }

    _consumeNew(cb)
    {
        check(cb,
              super.consumeNew(cb),
              super.prevConsumeStart);
    }

    consumeNew(cb)
    {
        if (cb)
        {
            return this._consumeNew(cb);
        }

        return this._consumeNewAsync();
    }

    _produceClaim(cb)
    {
        check(cb,
              super.produceClaim(cb),
              super.prevClaimStart,
              super.prevClaimEnd);
    }

    produceClaim(cb)
    {
        if (cb)
        {
            return this._produceClaim(cb);
        }

        return this._produceClaimAsync();
    }

    _produceClaimMany(n, cb)
    {
        check(cb,
              super.produceClaimMany(n, cb),
              super.prevClaimStart,
              super.prevClaimEnd);
    }

    produceClaimMany(n, cb)
    {
        if (cb)
        {
            return this._produceClaimMany(n, cb);
        }

        return this._produceClaimManyAsync(n);
    }

    _produceClaimAvail(max, cb)
    {
        check(cb,
              super.produceClaimAvail(max, cb),
              super.prevClaimStart,
              super.prevClaimEnd);
    }

    produceClaimAvail(max, cb)
    {
        if (cb)
        {
            return this._produceClaimAvail(max, cb);
        }

        return this._produceClaimAvailAsync(max);
    }

    _produceCommit(claimStart, claimEnd, cb)
    {
        if (arguments.length >= 2)
        {
            return check(cb, super.produceCommit(claimStart, claimEnd, cb));
        }

        check(claimStart, super.produceCommit(claimStart));
    }

    produceCommit(claimStart, claimEnd, cb)
    {
        if (arguments.length >= 2)
        {
            if (cb)
            {
                return this._produceCommit(claimStart, claimEnd, cb);
            }

            return this._produceCommitAsync(claimStart, claimEnd);
        }

        if (claimStart)
        {
            return this._produceCommit(claimStart);
        }

        return this._produceCommitAsync();
    }
}

class DisruptorReadStream extends Readable {
    constructor(disruptor, options) {
        super(options);
        if (disruptor.elementSize !== 1) {
            throw new Error('element size must be 1');
        }
        if (disruptor.spin) {
            throw new Error('spin must be false');
        }
        this.disruptor = disruptor;
        this._reading = false;
    }

    async __read() {
        if (this._reading) {
            return;
        }
        let buf;
        this._reading = true;
        try {
            buf = Buffer.concat((await this.disruptor.consumeNew()).bufs);
            this.disruptor.consumeCommit();
        }
        catch (ex) {
            this._reading = false; // must be done in case _destroy below gets called
            this.emit('error', ex);
        }
        this._reading = false;
        if (this._destroy_info) {
            const { err, cb } = this._destroy_info;
            cb(err);
            return;
        }
        return buf;
    }

    async _read() {
        if (this.destroyed) {
            return;
        }
        const buf = await this.__read();
        if (buf) {
            if (buf.length > 0) {
                if (this.push(buf)) {
                    this.read_again();
                }
            } else if (this.disruptor.status === status_eof) {
                // Check for data the writer added between us reading zero bytes
                // and it setting status_eof
                const buf2 = await this.__read();
                if (buf2) {
                    if (buf2.length > 0) {
                        if (this.push(buf2)) {
                            this.read_again();
                        }
                    } else {
                        this.push(null);
                    }
                }
            } else if (this.disruptor.status === status_error) {
                this.emit('error', new Error('writer errored'));
            } else {
                this.read_again();
            }
        }
    }

    read_again() {
        setImmediate(() => this._read());
    }

    _destroy(err, cb) {
        if (this._reading) {
            this._destroy_info = { err, cb };
        } else {
            cb(err);
        }
    }
}

class DisruptorWriteStream extends Writable {
    constructor(disruptor, options) {
        super(options);
        if (disruptor.elementSize !== 1) {
            throw new Error('element size must be 1');
        }
        if (disruptor.spin) {
            throw new Error('spin must be false');
        }
        this.disruptor = disruptor;
        this._writing = false;
    }

    async _write(chunk, encoding, cb) {
        if (this.destroyed) {
            return;
        }
        let bufs, claimStart, claimEnd;
        this._writing = true;
        try {
            ({ bufs, claimStart, claimEnd } = await this.disruptor.produceClaimAvail(chunk.length));
        } catch (ex) {
            this._writing = false; // must be done before onwrite called _destroy above
            return cb(ex);
        }
        this._writing = false;
        if (this._destroy_info) {
            const { err, cb } = this._destroy_info;
            return cb(err);
        }
        let i = 0;
        for (const buf of bufs) {
            chunk.copy(buf, 0, i, buf.length);
            i += buf.length;
        }
        if (i === 0) {
            return this.write_again(chunk, encoding, cb);
        }
        await this._commit(claimStart, claimEnd, chunk.slice(i), encoding, cb);
    }

    _final(cb) {
        this.disruptor.status = status_eof;
        cb();
    }

    _destroy(err, cb) {
        if (err) {
            this.disruptor.status = status_error;
        }
        if (this._writing) {
            this._destroy_info = { err, cb };
        } else {
            cb(err);
        }
    }

    async _commit(claimStart, claimEnd, chunk, encoding, cb) {
        if (this.destroyed) {
            return;
        }
        let committed;
        this._writing = true;
        try {
            committed = await this.disruptor.produceCommit(claimStart, claimEnd);
        } catch (ex) {
            this._writing = false; // must be done before onwrite called _destroy above
            return cb(ex);
        }
        this._writing = false;
        if (this._destroy_info) {
            const { err, cb } = this._destroy_info;
            return cb(err);
        }
        if (!committed) {
            return this.commit_again(claimStart, claimEnd, chunk, encoding, cb);
        }
        if (chunk.length > 0) {
            return this.write_again(chunk, encoding, cb);
        }
        cb();
    }

    commit_again(claimStart, claimEnd, chunk, encoding, cb) {
        setImmediate(() => this._commit(claimStart, claimEnd, chunk, encoding, cb));
    }

    write_again(chunk, encoding, cb) {
        setImmediate(() => this._write(chunk, encoding, cb));
    }
}

exports.Disruptor = Disruptor2;
exports.DisruptorReadStream = DisruptorReadStream;
exports.DisruptorWriteStream = DisruptorWriteStream;
