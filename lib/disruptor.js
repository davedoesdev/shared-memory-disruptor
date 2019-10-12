const { promisify } = require('util');
const Disruptor = require('bindings')('disruptor.node').Disruptor;

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

exports.Disruptor = Disruptor2;
