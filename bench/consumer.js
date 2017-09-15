let Disruptor = require('..').Disruptor;
let assert = require('assert');
let d = new Disruptor('/example', 1024 * 64, 4, 1, 0, false, true);
let i = 0;
let start = new Date();

while (i < 10000000)
{
    let bufs = d.consumeNewSync();

    for (let buf of bufs)
    {
        for (let j = 0; j < buf.length; j += 4)
        {
            assert.equal(buf.readUInt32LE(j, true), i++);
        }
    }
}

let end = new Date();

d.consumeCommit();

console.log(end - start);
