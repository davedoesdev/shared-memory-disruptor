let Disruptor = require('.').Disruptor;
let d = new Disruptor('/test', 1000, 4, 1, 0, true, true);

let num = 1000000;
let count = 0;
let sum = 0;

function produce()
{
    d.produceClaim(function (err, b)
    {
        if (err) { throw err; }
        let n = Math.floor(Math.random() * 100);
        b.writeUInt32LE(n, 0, true);
        d.produceCommit(b, function ()
        {
            sum += n;
            count++;
            if (count === num)
            {
                return console.log(sum);
            }
            produce();
        });
    });
}

produce();
