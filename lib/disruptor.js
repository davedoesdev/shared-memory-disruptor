let Disruptor = require('bindings')('disruptor.node').Disruptor;

function check(cb, r, arg1, arg2)
{
    if ((r !== undefined) && cb)
    {
        process.nextTick(cb, null, r, arg1, arg2);
    }
}

class Disruptor2 extends Disruptor
{
    consumeNew(cb)
    {
        check(cb,
              super.consumeNew(cb),
              super.prevConsumeStart,
              super.prevConsumeEnd);
    }

    produceClaim(cb)
    {
        check(cb,
              super.produceClaim(cb),
              super.prevClaimStart,
              super.prevClaimEnd);
    }

    produceClaimMany(n, cb)
    {
        check(cb,
              super.produceClaimMany(n, cb),
              super.prevClaimStart,
              super.prevClaimEnd);
    }

    produceCommit(claimStart, claimEnd, cb)
    {
        if (arguments.length >= 2)
        {
            return check(cb, super.produceCommit(claimStart, claimEnd, cb));
        }

        check(claimStart, super.produceCommit(claimStart));
    }
}

exports.Disruptor = Disruptor2;
