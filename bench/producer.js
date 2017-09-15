let Disruptor = require('..').Disruptor;
let d = new Disruptor('/example', 1024 * 64, 4, 1, -1, true, true);

for (let i = 0; i < 10000000; i += 1)
{
    let buf = d.produceClaimSync();
    buf.writeUInt32LE(i, 0, true);
    d.produceCommitSync();
}
