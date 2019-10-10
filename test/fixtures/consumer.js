let worker_threads = require('worker_threads'),
    argv = worker_threads.workerData || require('yargs').argv,
    Disruptor = require('../..').Disruptor,
    d = new Disruptor('/test', 1000, 256, argv.num_consumers, argv.n, false, true),
    count = 0,
    sum = 0;

while (count !== argv.num_producers * argv.num_elements_to_write)
{
    let bs = d.consumeNewSync();

    for (let b of bs)
    {
        count += b.length / 256;

        for (let i = 0; i < b.length; i += 1)
        {
            sum += b[i];
        }
    }

    d.consumeCommit();
}

if (worker_threads.parentPort)
{
    worker_threads.parentPort.postMessage(sum);
}
else
{
    process.send(sum);
}
