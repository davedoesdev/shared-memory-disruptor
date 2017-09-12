let Disruptor = require('bindings')('disruptor.node').Disruptor;

function check(r, cb)
{
    if ((r !== undefined) && cb)
    {
        process.nextTick(cb, null, r);
    }
}

class Disruptor2 extends Disruptor
{
    consumeNew(cb)
    {
        check(super.consumeNew(cb), cb);
    }

    produceClaim(cb)
    {
        check(super.produceClaim(cb), cb);
    }

    produceClaimMany(n, cb)
    {
        check(super.produceClaimMany(n, cb), cb);
    }

    produceCommit(b, cb)
    {
        check(super.produceCommit(b, cb), cb);
    }
}

exports.Disruptor = Disruptor2;
