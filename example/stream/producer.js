// You must run the consumer before the producer
// See README.md for examples

const { Disruptor, DisruptorWriteStream } = require('../../');

// The producer needs to be aware of the number of consumers
const total = +(process.env.TOTAL || 1)

const d = new Disruptor('/stream', 1000, 1, total, 0, false, false);
const ws = new DisruptorWriteStream(d);

process.stdin.pipe(ws);
