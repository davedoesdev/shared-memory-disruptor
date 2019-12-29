let worker_threads = require('worker_threads'),
    path = require('path'),
    Disruptor = require('..').Disruptor,
    expect = require('chai').expect,
    async = require('async');

function many(num_producers, num_consumers, num_elements_to_write)
{
describe('multi-workers many-to-many (producers: ' + num_producers + ', consumers: ' + num_consumers + ', elements to write: ' + num_elements_to_write + ')', function ()
{
    this.timeout(10 * 60 * 1000);

    it('should transfer data', function (done)
    {
        (new Disruptor('/test', 1000, 256, num_consumers, 0, true, true)).release();

        let csums = null, psums = null;

        function check()
        {
            if (csums && psums)
            {
                let sum = 0;
                for (let s of psums)
                {
                    sum += s;
                }

                for (let s of csums)
                {
                    expect(s).to.equal(sum);
                }

                done();
            }
        }

        async.times(num_consumers, async.ensureAsync(function (n, next)
        {
            const worker = new worker_threads.Worker(
                path.join(__dirname, 'fixtures', 'consumer.js'),
                {
                    workerData: {
                        num_producers,
                        num_consumers,
                        num_elements_to_write,
                        n
                    }
                });

            worker.on('message', function (sum)
            {
                next(null, sum);
            });
        }), function (err, sums)
        {
            if (err) { return done(err); }
            csums = sums;
            check();
        });

        async.times(num_producers, async.ensureAsync(function (n, next)
        {
            const worker = new worker_threads.Worker(
                path.join(__dirname, 'fixtures', 'producer.js'),
                {
                    workerData: {
                        num_consumers,
                        num_elements_to_write
                    }
                });

            worker.on('message', function (sum)
            {
                next(null, sum);
            });
        }), function (err, sums)
        {
            if (err) { return done(err); }
            psums = sums;
            check();
        });
    });
});
}

for (let num_producers of [1, 2, 4])
{
    for (let num_consumers of [1, 2, 4])
    {
        for (let num_elements_to_write of [1, 2, 10, 100, 1000, 10000])
        {
            many(num_producers, num_consumers, num_elements_to_write);
        }
    }
}
