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
            return d['produceCommit' + async_suffix](b, cb)
        }

        cb(null, d.produceCommitSync(b));
    }

describe('single process (async=' + do_async + ', async_suffix=' + async_suffix + ')', function ()
{
    describe('single value', function ()
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
                produceCommit(d, b, function (err)
                {
                    if (err) { return done(err); }
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
            }), function (err)
            {
                if (err) { return done(err); }
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
            }), function (err)
            {
                if (err) { return done(err); }
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
                    }), function (err)
                    {
                        if (err) { return done(err); }
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
    });
});
}

tests(false);
tests(true, '');
tests(true, 'Async');

// Get code coverage of JS and C++ working
// set bounds etc to fill in code coverage
// strings
// spin
// do async.times, not series - with a lot we should get some contention
//   (i.e. with many producers)
// > 1 consumer, producer
// do multi-process test

