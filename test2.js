var Disruptor = require('.').Disruptor;
var d = new Disruptor('/test', 100, 4, 1, 0, false);

let sum = 0;
let i = 0;
while (i < 11000)
{
    var bs = d.consumeNewSync();
    for (let b of bs)
    {
        for (let j = 0; j < b.length; j += 4)
        {
            sum += b.readUInt32LE(j, true);
            i += 1;
        }
    }
    d.consumeCommit();
}
console.log(sum);
