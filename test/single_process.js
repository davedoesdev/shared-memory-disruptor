/* jshint expr: true */

let Disruptor = require('..').Disruptor,
    expect = require('chai').expect,
    async = require('async');

function tests(do_async, async_suffix)
{
    function consumeNew(d, cb)
    {
        if (do_async)
        {
            return d['consumeNew' + async_suffix](cb);
        }

        cb(null, d.consumeNewSync());
    }

    function consumeCommit(d)
    {
        return d.consumeCommit();
    }

    function produceClaim(d, cb)
    {
        if (do_async)
        {
            return d['produceClaim' + async_suffix](cb);
        }

        cb(null, d.produceClaimSync());
    }

    function produceCommit(d, b, cb)
    {
        if (do_async)
        {
            return d['produceCommit' + async_suffix](b, cb);
        }

        if (cb)
        {
            return cb(null, d.produceCommitSync(b));
        }

        d.produceCommitSync(b);
    }

describe('functionality and state (async=' + do_async + ', async_suffix=' + async_suffix + ')', function ()
{
    let d;

    beforeEach(function ()
    {
        d = new Disruptor('/test', 256, 8, 1, 0, true, false);
    });

    afterEach(function ()
    {
        d.release();
    });

    it('should zero out initial values', function ()
    {
        expect(d.cursor).to.equal(0);
        expect(d.next).to.equal(0);
        expect(d.consumer).to.equal(0);
        expect(d.consumers.equals(Buffer.alloc(8))).to.be.true;
        expect(d.elements.equals(Buffer.alloc(256 * 8))).to.be.true;
    });

    it('should write and read single value', function (done)
    {
        produceClaim(d, function (err, b)
        {
            if (err) { return done(err); }
            expect(b.equals(Buffer.alloc(8))).to.be.true;
            b.writeUInt32BE(0x01234567, 0, true);
            b.writeUInt32BE(0x89abcdef, 4, true);
            expect(d.cursor).to.equal(0);
            expect(d.next).to.equal(1);
            expect(d.consumer).to.equal(0);
            produceCommit(d, b, function (err, v)
            {
                if (err) { return done(err); }
                expect(v).to.be.true;
                expect(d.cursor).to.equal(1);
                expect(d.next).to.equal(1);
                expect(d.consumer).to.equal(0);
                consumeNew(d, function (err, b2)
                {
                    if (err) { return done(err); }
                    expect(b2.length).to.equal(1);
                    expect(b2[0].equals(Buffer.from([0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]))).to.be.true;
                    expect(d.cursor).to.equal(1);
                    expect(d.next).to.equal(1);
                    expect(d.consumer).to.equal(0);
                    consumeCommit(d);
                    expect(d.cursor).to.equal(1);
                    expect(d.next).to.equal(1);
                    expect(d.consumer).to.equal(1);
                    done();
                });
            });
        });
    });

    it('should return empty array if nothing to consume', function (done)
    {
        consumeNew(d, function (err, b)
        {
            if (err) { return done(err); }
            expect(b).to.eql([]);
            expect(d.consumer).to.equal(0);
            done();
        });
    });

    it('should fill up', function (done)
    {
        async.timesSeries(256, async.ensureAsync(function (n, next)
        {
            produceClaim(d, function (err, b)
            {
                if (err) { return next(err); }
                expect(b.length).to.equal(8);
                b.writeUInt32BE(0x01234567, 0, true);
                b.writeUInt32BE(0x89abcdef, 4, true);
                produceCommit(d, b, next);
            });
        }), function (err, vs)
        {
            if (err) { return done(err); }
            expect(vs).to.eql(Array(256).fill(true));
            expect(d.cursor).to.equal(256);
            expect(d.next).to.equal(256);
            expect(d.consumer).to.equal(0);
            for (let i = 0; i < 256; i += 1)
            {
                expect(d.elements.slice(i * 8, i * 8 + 8).equals(Buffer.from([0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]))).to.be.true;
            }
            produceClaim(d, function (err, b)
            {
                if (err) { return next(err); }
                expect(b.length).to.equal(0);
                expect(d.cursor).to.equal(256);
                expect(d.next).to.equal(256);
                expect(d.consumer).to.equal(0);
                consumeNew(d, function (err, bs)
                {
                    if (err) { return next(err); }
                    expect(bs.length).to.equal(1);
                    expect(bs[0].equals(d.elements)).to.be.true;
                    expect(d.cursor).to.equal(256);
                    expect(d.next).to.equal(256);
                    expect(d.consumer).to.equal(0);
                    produceClaim(d, function (err, b)
                    {
                        if (err) { return next(err); }
                        expect(b.length).to.equal(0);
                        expect(d.cursor).to.equal(256);
                        expect(d.next).to.equal(256);
                        expect(d.consumer).to.equal(0);
                        consumeNew(d, function (err, bs2)
                        {
                            if (err) { return next(err); }
                            expect(bs2.length).to.equal(0);
                            expect(d.cursor).to.equal(256);
                            expect(d.next).to.equal(256);
                            expect(d.consumer).to.equal(256);
                            produceClaim(d, function (err, b3)
                            {
                                if (err) { return next(err); }
                                expect(b3.length).to.equal(8);
                                b3[0] = 0xff;
                                expect(d.elements[0]).to.equal(0xff);
                                done();
                            });
                        });
                    });
                });
            });
        });
    });

    it('should wrap around', function (done)
    {
        expect(d.cursor).to.equal(0);
        expect(d.next).to.equal(0);
        expect(d.consumer).to.equal(0);
        expect(d.consumers.equals(Buffer.alloc(8))).to.be.true;
        expect(d.elements.equals(Buffer.alloc(256 * 8))).to.be.true;

        async.timesSeries(200, async.ensureAsync(function (n, next)
        {
            produceClaim(d, function (err, b)
            {
                if (err) { return next(err); }
                expect(b.length).to.equal(8);
                b.writeUInt32BE(0x01234567, 0, true);
                b.writeUInt32BE(0x89abcdef, 4, true);
                produceCommit(d, b, next);
            });
        }), function (err, vs)
        {
            if (err) { return done(err); }
            expect(vs).to.eql(Array(200).fill(true));
            expect(d.cursor).to.equal(200);
            expect(d.next).to.equal(200);
            expect(d.consumer).to.equal(0);
            for (let i = 0; i < 256; i += 8)
            {
                if (i < 200)
                {
                    expect(d.elements.slice(i * 8, i * 8 + 8).equals(Buffer.from([0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]))).to.be.true;
                }
                else
                {
                    expect(d.elements.slice(i * 8, i * 8 + 8).equals(Buffer.alloc(8))).to.be.true;
                }
            }
            consumeNew(d, function (err, bs)
            {
                if (err) { return next(err); }
                expect(bs.length).to.equal(1);
                expect(bs[0].length).to.equal(200 * 8);
                expect(d.cursor).to.equal(200);
                expect(d.next).to.equal(200);
                expect(d.consumer).to.equal(0);

                for (let i = 0; i < 200; i += 8)
                {
                    expect(bs[0].slice(i * 8, i * 8 + 8).equals(Buffer.from([0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]))).to.be.true;
                }

                consumeCommit(d);
                expect(d.consumer).to.equal(200);

                async.timesSeries(86, async.ensureAsync(function (n, next)
                {
                    produceClaim(d, function (err, b)
                    {
                        if (err) { return next(err); }
                        expect(b.length).to.equal(8);
                        b.writeUInt32BE(0x5a5a5a5a, 0, true);
                        b.writeUInt32BE(0x5a5a5a5a, 4, true);
                        produceCommit(d, b, next);
                    });
                }), function (err, vs)
                {
                    if (err) { return done(err); }
                    expect(vs).to.eql(Array(86).fill(true));
                    expect(d.cursor).to.equal(286);
                    expect(d.next).to.equal(286);
                    expect(d.consumer).to.equal(200);
                    for (let i = 0; i < 256; i += 8)
                    {
                        if ((i < 30) || (i >= 200))
                        {
                            expect(d.elements.slice(i * 8, i * 8 + 8).equals(Buffer.alloc(8, 0x5a))).to.be.true;
                        }
                        else
                        {
                            expect(d.elements.slice(i * 8, i * 8 + 8).equals(Buffer.from([0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]))).to.be.true;
                        }
                    }

                    consumeNew(d, function (err, bs2)
                    {
                        expect(bs2.length).to.equal(2);
                        expect(bs2[0].equals(Buffer.alloc(56 * 8, 0x5a))).to.be.true;
                        expect(bs2[1].equals(Buffer.alloc(30 * 8, 0x5a))).to.be.true;
                        done();
                    });
                });
            });
        });
    });

    it('should throw error if invalid name passed', function ()
    {
        expect(function ()
        {
            new Disruptor('', 256, 8, 1, 0, true, false);
        }).to.throw('Failed to open shared memory object: Invalid argument');
    });

    it('should cope with no callback', function (done)
    {
        produceClaim(d, function (err, b)
        {
            if (err) { return done(err); }
            expect(b.equals(Buffer.alloc(8))).to.be.true;
            b.writeUInt32BE(0x01234567, 0, true);
            b.writeUInt32BE(0x89abcdef, 4, true);
            expect(d.cursor).to.equal(0);
            expect(d.next).to.equal(1);
            expect(d.consumer).to.equal(0);
            produceCommit(d, b);
            setTimeout(function ()
            {
                expect(d.cursor).to.equal(1);
                expect(d.next).to.equal(1);
                expect(d.consumer).to.equal(0);
                done();
            }, 500);
        });
    });

    it('should return empty array if consume when empty', function (done)
    {
        consumeNew(d, function (err, b)
        {
            if (err) { return done(err); }
            expect(b.length).to.equal(0);
            done();
        });
    });

    it('should return empty buffer if produce when full', function (done)
    {
        async.timesSeries(256, async.ensureAsync(function (n, next)
        {
            produceClaim(d, function (err, b)
            {
                if (err) { return next(err); }
                expect(Buffer.isBuffer(b)).to.be.true;
                expect(b.length).to.equal(8);
                produceCommit(d, b, next);
            });
        }), function (err, vs)
        {
            if (err) { return done(err); }
            expect(vs).to.eql(Array(256).fill(true));
            expect(d.cursor).to.equal(256);
            expect(d.next).to.equal(256);
            expect(d.consumer).to.equal(0);
            produceClaim(d, function (err, b)
            {
                if (err) { return next(err); }
                expect(Buffer.isBuffer(b)).to.be.true;
                expect(b.length).to.equal(0);
                done();
            });
        });
    });

    it("should return false if other producers haven't committed", function (done)
    {
        produceClaim(d, function (err, b)
        {
            if (err) { return next(err); }
            expect(Buffer.isBuffer(b)).to.be.true;
            expect(b.length).to.equal(8);
            expect(b.seq_next).to.equal(0);
            expect(d.cursor).to.equal(0);
            expect(d.next).to.equal(1);
            expect(d.consumer).to.equal(0);
            produceClaim(d, function (err, b2)
            {
                if (err) { return next(err); }
                expect(Buffer.isBuffer(b2)).to.be.true;
                expect(b2.length).to.equal(8);
                expect(b2.seq_next).to.equal(1);
                expect(d.cursor).to.equal(0);
                expect(d.next).to.equal(2);
                expect(d.consumer).to.equal(0);
                produceCommit(d, b2, function (err, v)
                {
                    if (err) { return next(err); }
                    expect(v).to.equal(false);
                    expect(d.cursor).to.equal(0);
                    expect(d.next).to.equal(2);
                    expect(d.consumer).to.equal(0);
                    produceCommit(d, b, function (err, v2)
                    {
                        if (err) { return next(err); }
                        expect(v2).to.equal(true);
                        expect(d.cursor).to.equal(1);
                        expect(d.next).to.equal(2);
                        expect(d.consumer).to.equal(0);
                        produceCommit(d, b2, function (err, v3)
                        {
                            if (err) { return next(err); }
                            expect(v3).to.equal(true);
                            expect(d.cursor).to.equal(2);
                            expect(d.next).to.equal(2);
                            expect(d.consumer).to.equal(0);
                            done();
                        });
                    });
                });
            });
        });
    });
});
}

tests(false);
tests(true, '');
tests(true, 'Async');

describe('async spin', function ()
{
    this.timeout(60000);

    it('should spin when full', function (done)
    {
        let d = new Disruptor('/test', 1, 1, 1, 0, true, true);

        d.produceClaim(function (err, b)
        {
            let called = false;

            if (err) { return done(err); }
            d.produceClaim(function (err, b)
            {
                if (err) { return done(err); }
                called = true;
                d.release();
                done();
            });

            b[0] = 90;

            setTimeout(function ()
            {
                expect(called).to.be.false;
                d.produceCommit(b, function (err, v)
                {
                    if (err) { return done(err); }
                    expect(v).to.be.true;
                    expect(called).to.be.false;
                    d.consumeNew(function (err, b)
                    {
                        if (err) { return done(err); }
                        expect(b.length).to.equal(1);
                        expect(b[0].equals(Buffer.from([90]))).to.be.true;
                        d.consumeCommit();
                    });
                });
            }, 2000);
        });
    });

    it('should spin when empty', function (done)
    {
        let d = new Disruptor('/test', 1, 1, 1, 0, true, true);

        let called = false;

        d.consumeNew(function (err, b)
        {
            if (err) { return done(err); }
            called = true;
            d.release();
            done();
        });

        setTimeout(function ()
        {
            expect(called).to.be.false;
            d.produceClaim(function (err, b)
            {
                if (err) { return done(err); }
                expect(called).to.be.false;
                d.produceCommit(b, function (err, v)
                {
                    if (err) { return done(err); }
                    expect(v).to.be.true;
                });
            });
        }, 2000);
    });

    it("should spin if other producers haven't committed", function (done)
    {
        let d = new Disruptor('/test', 2, 1, 1, 0, true, true);

        let called = false;

        d.produceClaim(function (err, b)
        {
            if (err) { return done(err); }

            d.produceClaim(function (err, b2)
            {
                if (err) { return done(err); }

                d.produceCommit(b2, function (err, v)
                {
                    if (err) { return done(err); }
                    expect(v).to.be.true;
                    called = true;
                    d.release();
                    done();
                });

                setTimeout(function ()
                {
                    expect(called).to.be.false;
                    d.produceCommit(b, function (err, v)
                    {
                        if (err) { return done(err); }
                        expect(v).to.be.true;
                    });
                }, 2000);
            });
        });
    });
});

// strings
// > 1 consumer, producer
// spin (multi-process)
// do multi-process test

