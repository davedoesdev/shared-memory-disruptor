// You must run the consumer before the producer
// See README.md for examples

const { Disruptor, DisruptorReadStream } = require('../../');

// Allow reusing this consumer by providing TOTAL=n and ID from 0 to TOTAL-1
const id = +(process.env.ID || 0);
const total = +(process.env.TOTAL || id + 1);
const init = id === 0;

const d = new Disruptor('/stream', 1000, 1, total, id, init, false);

const rs = new DisruptorReadStream(d);
rs.pipe(process.stdout);