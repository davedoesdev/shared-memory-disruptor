let crypto = require('crypto'),
    argv = require('yargs').argv,
    Disruptor = require('../..').Disruptor;
    d = new Disruptor('/test', 100000, 256, argv.num_consumers, 0, false, true),
    sum = 0;

for (let i = 0; i < argv.num_elements_to_write; i += 1)
{
    let b = d.produceClaimSync();

    crypto.randomFillSync(b);

    for (let j = 0; j < b.length; j += 1)
    {
        sum += b[j];
    }

    d.produceCommitSync(b);
}

process.send(sum);
