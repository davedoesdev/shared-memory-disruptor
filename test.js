let Disruptor = require('.').Disruptor;
let d = new Disruptor('/test', 100, 4, 1, 0, true, 0);

let sum = 0;
for (let i = 0; i < 11000; i += 1)
{
    let n = Math.floor(Math.random() * 100);
    let b = d.produceClaimSync();
    b.writeUInt32LE(n, 0, true);
    d.produceCommitSync(b);
    sum += n;
}
console.log(sum);
