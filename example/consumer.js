let Disruptor = require('shared-memory-disruptor').Disruptor;
let d = new Disruptor('/example', 1000, 4, 1, 0, false, true);
let sum = 0;
let i = 0;

while (i < 1000000)
{
    let bufs = d.consumeNewSync();

    for (let buf of bufs)
    {
        for (let j = 0; j < buf.length; j += 4)
        {
            sum += buf.readUInt32LE(j, true);
            i += 1;
        }
    }

    d.consumeCommit();
}

console.log(sum);
