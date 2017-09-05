let Disruptor = require('..').Disruptor,
    expect = require('chai').expect;

function tests(async, async_suffix)
{
    function consumeNew(d, cb)
    {
        if (async)
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
        if (async)
        {
            return d['produceClaim' + async_suffix](cb);
        }

        cb(null, d.produceClaimSync());
    }

    function produceCommit(d, b, cb)
    {
        if (async)
        {
            return d['produceCommit' + async_suffix](b, cb)
        }

        cb(null, d.produceCommitSync(b));
    }

describe('single process (async=' + async + ', async_suffix=' + async_suffix + ')', function ()
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
    });
});
}

tests(false);
tests(true, '');
tests(true, 'Async');

// Get code coverage of JS and C++ working
// set bounds etc to fill in code coverage
// strings
// do multi-process test

