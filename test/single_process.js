let Disruptor = require('..').Disruptor,
    expect = require('chai').expect;

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

    it('should write and read single value', function ()
    {
        let b = d.produceClaimSync();
        expect(b.equals(Buffer.alloc(8))).to.be.true;
        b.writeUInt32BE(0x01234567, 0, true);
        b.writeUInt32BE(0x89abcdef, 4, true);
        expect(d.cursor).to.equal(0);
        expect(d.next).to.equal(1);
        expect(d.consumer).to.equal(0);
        d.produceCommit(b);
        expect(d.cursor).to.equal(1);
        expect(d.next).to.equal(1);
        expect(d.consumer).to.equal(0);

        let b2 = d.consumeNewSync();
        expect(b2.length).to.equal(1);
        expect(b2[0].equals(Buffer.from([0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]))).to.be.true;
        expect(d.cursor).to.equal(1);
        expect(d.next).to.equal(1);
        expect(d.consumer).to.equal(0);
        d.consumeCommit();
        expect(d.cursor).to.equal(1);
        expect(d.next).to.equal(1);
        expect(d.consumer).to.equal(1);
    });

});

// Get code coverage of JS and C++ working
// set bounds etc to fill in code coverage
// strings
// do multi-process test

