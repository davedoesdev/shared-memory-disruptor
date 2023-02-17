const {
    Disruptor,
    DisruptorWriteStream
} = require('..');

const ws = new DisruptorWriteStream(
    new Disruptor('/stream', 5000, 1, 1, 0, false, false)
);

function push(data) {
    return new Promise((resolve) => {
        ws.write(JSON.stringify(data) + '\0', function () {
            resolve();
        });
    });
}

(async function () {
    for (let i = 0; i < 1000; i++) {
        await push({test: "Text", data: i});
    }
    ws.end();
})();
