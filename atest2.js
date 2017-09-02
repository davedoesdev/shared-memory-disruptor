let Disruptor = require('.').Disruptor;
let d = new Disruptor('/test', 1000, 4, 1, 0, false, true);

let num = 1000000;
let count = 0;
let sum = 0;

function consume()
{
    d.consumeNew(function (err, bs)
    {
        if (err) { throw err; }
        for (let b of bs)
        {
            for (let j = 0; j < b.length; j += 4)
            {
                sum += b.readUInt32LE(j, true);
                count += 1;
            }
        }
        d.consumeCommit();
        if (count === num)
        {
            return console.log(sum);
        }
        consume();
    });
}

consume();
