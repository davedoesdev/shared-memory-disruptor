// Example using a stream of random numbers:
//
//    { while true; do echo -n $RANDOM; done; } | node producer.js
//
// You can pipe arbitrary data

const {
    Disruptor,
    DisruptorWriteStream
} = require('../../');

const d = new Disruptor('/stream', 1000, 1, 1, 0, true, false);
const ws = new DisruptorWriteStream(d);

process.stdin.pipe(ws);
