let Disruptor = require('shared-memory-disruptor').Disruptor;
let d = new Disruptor('/example', 1000, 4, 1, -1, true, true);
let sum = 0;

for (let i = 0; i < 1000000; i += 1)
{
    let n = Math.floor(Math.random() * 100);
    let buf = d.produceClaimSync();
    buf.writeUInt32LE(n, 0, true);
    d.produceCommitSync(buf);
    sum += n;
}

console.log(sum);
