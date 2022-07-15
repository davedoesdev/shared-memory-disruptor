const {
    Disruptor,
    DisruptorReadStream
} = require('../../');

const d = new Disruptor('/stream', 1000, 1, 1, 0, true, false);
const rs = new DisruptorReadStream(d);
rs.pipe(process.stdout);