/**
  Creates an object which uses an
  {@link https://lmax-exchange.github.io/disruptor/|LMAX Disruptor}
  to store data in shared memory.

  @param {string} shm_name - Name of shared memory object to use (see {@link http://pubs.opengroup.org/onlinepubs/009695399/functions/shm_open.html|shm_open}).
  @param {integer} num_elements - Number of elements in the Disruptor (i.e. its capacity).
  @param {integer} element_size - Size of each element in bytes.
  @param {integer} num_consumers - Total number of objects that will be reading data from the Disruptor.
  @param {integer} consumer - Each object that reads data from the Disruptor must have a unique ID. This should be a number between 0 and `num_consumers - 1`. If the object is only going to write data, `consumer` can be anything.
  @param {boolean} init - Whether to create and initialize the shared memory backing the Disruptor. You should arrange your application so this is done once, at the start.
  @param {boolean} spin - If `true` then methods on this object which read from the Disruptor won't return to your application until a value is ready. Methods which write to the Disruptor won't return while the Disruptor is full. The `*Sync` methods will block Node's main thread and the asynchronous methods will repeatedly post tasks to the thread pool, in order to let other tasks get a look in. If you want to implement your own retry algorithm (or use some out-of-band notification mechanism), specify `spin` as `false` and check method return values.
 */
class Disruptor
{
    constructor(shm_name, num_elements, element_size, num_consumers, consumer, init, spin)
    {
    }

    /**
      Reserve the next element in the Disruptor for writing data into.

      @param {produceClaimCallback} [cb] - Called once an element has been reserved, or `spin` (see the {@link Disruptor|constructor}) is `false` and the Disruptor is full. If you don't pass `cb` then a `Promise`.
      @returns {undefined | Promise} - If `cb` is _not_ supplied then a `Promise` is returned which resolves to the data that would have been passed to it.
     */
    produceClaim(cb)
    {
    }

    /**
      Reserve the next element in the Disruptor for writing data into.

      @returns {Buffer} - Buffer for writing data to the Disruptor. If the Disruptor was full and `spin` (see the {@link Disruptor|constructor}) is `false`, the buffer will be empty. Otherwise its length will be `element_size`. The buffer is backed by shared memory so may be overwitten after you call {@link Disruptor#produceCommit|produceCommit} or {@link Disruptor#produceCommitSync|produceCommitSync}.
     */
    produceClaimSync()
    {
    }

    /**
      Reserve a number of elements in the Disruptor for writing data into.

      @param {integer} n - Number of elements to reserve.
      @param {produceClaimManyCallback} [cb] - Called once the elements have been reserved, or `spin` (see the {@link Disruptor|constructor}) is `false` and the Disruptor didn't have enough free elements.
      @returns {undefined | Promise} - If `cb` is _not_ supplied then a `Promise` is returned which resolves to the data that would have been passed to it.
     */
    produceClaimMany(n, cb)
    {
    }

    /**
      Reserve a number of elements in the Disruptor for writing data into.

      @param {integer} n - Number of elements to reserve.
      @returns {Buffer[]} - Array of buffers for writing data to the Disruptor. If the Disruptor didn't have enough free elements and `spin` (see the {@link Disruptor|constructor}) is `false`, the array will be empty. Otherwise, it will contain at least one buffer and each buffer will be a multiple of `element_size` in length. The total size of the buffers in the array will be `n * element_size`. The buffers are backed by shared memory so may be overwritten after you call {@link Disruptor#produceCommit|produceCommit} or {@link Disruptor#produceCommitSync|produceCommitSync}.
     */
    produceClaimManySync(n)
    {
    }

    /**
      Reserve all free elements in the Disruptor for writing data into.

      @param {integer} max - Maximum number of free elements to reserve.
      @param {produceClaimManyCallback} [cb] - Called once elements have been reserved, or `spin` (see the {@link Disruptor|constructor}) is `false` and the Disruptor didn't have any free elements.
      @returns {undefined | Promise} - If `cb` is _not_ supplied then a `Promise` is returned which resolves to the data that would have been passed to it.
     */
    produceClaimAvail(max, cb)
    {
    }

    /**
      Reserve all free elements in the Disruptor for writing data into.

      @param {integer} max - Maximum number of free elements to reserve.
      @returns {Buffer[]} - Array of buffers for writing data to the Disruptor. If the Disruptor didn't have any free elements and `spin` (see the {@link Disruptor|constructor}) is `false`, the array will be empty. Otherwise, it will contain at least one buffer and each buffer will be a multiple of `element_size` is length. The buffers are backed by shared memory so may be overwritten after you call {@link Disruptor#produceCommit|produceCommit} or {@link Disruptor#produceCommitSync|produceCommitSync}.
     */
    produceClaimAvailSync(max)
    {
    }


    /**
      Commit data to the Disruptor. Call this once you've finished writing data
      to buffers you reserved.

      @param {integer} [claimStart] - Specifies the start of the buffer you want to commit. You can pass a value you received via {@link produceClaimCallback}, {@link produceClaimManyCallback} or {@link produceClaimAvailCallback}. If you don't specify a value, {@link Disruptor#prevClaimStart|prevClaimStart} is used.
      @param {integer} [claimEnd] - Specifies the end of the buffer you want to commit. You can pass a value you received via {@link produceClaimCallback}, {@link produceClaimManyCallback} or {@link produceClaimAvailCallback}. If you don't specify a value, {@link Disruptor#prevClaimEnd|prevClaimEnd} is used.
      @param {produceCommitCallback} [cb] - Called once the elements in the buffers have been committed to the Disruptor, or `spin` (see the {@link Disruptor|constructor}) is `false` and the elements couldn't be committed (because other producers haven't committed their data yet). No copying occurs during the operation.
      @returns {undefined | Promise} - If `cb` is _not_ supplied then a `Promise` is returned which resolves to the data that would have been passed to it.
     */
    produceCommit(claimStart, claimEnd, cb)
    {
    }

    /**
      Commit data to the Disruptor. Call this once you've finished writing data
      to buffers you reserved.

      @param {integer} [claimStart] - Specifies the start of the buffer you want to commit. You can pass a value you received via {@link produceClaimCallback}, {@link produceClaimManyCallback} or {@link produceClaimAvailCallback}. If you don't specify a value, {@link Disruptor#prevClaimStart|prevClaimStart} is used.
      @param {integer} [claimEnd] - Specifies the end of the buffer you want to commit. You can pass a value you received via {@link produceClaimCallback}, {@link produceClaimManyCallback} or {@link produceClaimAvailCallback}. If you don't specify a value, {@link Disruptor#prevClaimEnd|prevClaimEnd} is used.
      @returns {boolean} - Whether the data was committed to the Disruptor. If some elements reserved before `claimStart` remain uncommitted and `spin` (see the {@link Disruptor|constructor}) is `false`, the return value will be `false`. Otherwise the data was committed and the return value will be `true`.
      */
    produceCommitSync(claimStart, claimEnd)
    {
    }

    /**
      Read new data from the Disruptor.

      Elements written to since {@link Disruptor#consumeNew|consumeNew}, {@link Disruptor#consumeNewSync|consumeNewSync} or
      {@link Disruptor#consumeCommit|consumeCommit} were called on this object are passed to `cb`.

      A call to {@link Disruptor#consumeCommit|consumeCommit} is made before checking for new data.

      @param {consumeNewCallback} [cb] - Called once new elements are ready, or `spin` (see the {@link Disruptor|constructor}) is `false` and no elements have been written to the Disruptor.
      @returns {undefined | Promise} - If `cb` is _not_ supplied then a `Promise` is returned which resolves to the data that would have been passed to it.
     */
    consumeNew(cb)
    {
    }

    /**
      Read new data from the Disruptor.

      Elements written to since {@link Disruptor#consumeNew|consumeNew}, {@link Disruptor#consumeNewSync|consumeNewSync} or
      {@link Disruptor#consumeCommit|consumeCommit} were called on this object are returned.

      A call to {@link Disruptor#consumeCommit|consumeCommit} is made before checking for new data.

      @returns {Buffer[]} - Array of buffers containing new data ready to read from the Disruptor. If no new data was available and `spin` (see the {@link Disruptor|constructor}) is `false`, the array will be empty. Otherwise it will contain at least one buffer and each buffer will be a multiple of `element_size` in length. The buffers are backed by shared memory so may be overwritten after you call {@link Disruptor#consumeCommit|consumeCommit}.
     */
    consumeNewSync()
    {
    }

    /**
      Tell the Disruptor you've finished reading data. Call this once you've
      finished with buffers returned by {@link Disruptor#consumeNew|consumeNew} or
      {@link Disruptor#consumeNewSync|consumeNewSync}.

      This is called by {@link Disruptor#consumeNew|consumeNew} and {@link Disruptor#consumeNewSync|consumeNewSync} before
      they check for new data.

      @return {boolean} - Whether the Disruptor was in the expected state. This will always return `true` unless your application incorrectly uses objects with the same consumer ID to access the Disruptor concurrently.
     */
    consumeCommit()
    {
    }

    /**
      Reserve elements you've reserved before.

      @param {integer} claimStart - Specifies the start of the buffer you want to reserve. You can pass a value you received via {@link produceClaimCallback}, {@link produceClaimManyCallback}, {@link produceClaimAvailCallback} or {@link Disruptor#prevClaimStart|prevClaimStart}.
      @param {integer} claimEnd - Specifies the end of the buffer you want to reserve. You can pass a value you received via {@link produceClaimCallback}, {@link produceClaimManyCallback}, {@link produceClaimAvailCallback} or {@link Disruptor#prevClaimEnd|prevClaimEnd}.
     */
    produceRecover(claimStart, claimEnd)
    {
    }

    /**
      Detaches from the shared memory backing the Disruptor.

      Although this will be called when the object is garbage collected,
      you can force the shared memory to be unmapped by calling this function.

      Don't use the object again afterwards!
     */
    release()
    {
    }

    /**
      @returns {integer} - The Disruptor maintains a strictly increasing count of the total number of elements produced since it was created. This is how many elements were produced _before_ the previous call to {@link Disruptor#produceClaim|produceClaim}, {@link Disruptor#produceClaimSync|produceClaimSync}, {@link Disruptor#produceClaimMany|produceClaimMany}, {@link Disruptor#produceClaimManySync|produceClaimManySync}, {@link Disruptor#produceClaimAvail|produceClaimAvail} or {@link Disruptor#produceClaimAvailSync|produceClaimAvailSync}.
     */
    get prevClaimStart()
    {
    }

    /**
      @returns {integer} - The Disruptor maintains a strictly increasing count of the total number of elements produced since it was created. This is how many elements were produced _after_ the previous call to {@link Disruptor#produceClaim|produceClaim}, {@link Disruptor#produceClaimSync|produceClaimSync}, {@link Disruptor#produceClaimMany|produceClaimMany}, {@link Disruptor#produceClaimManySync|produceClaimManySync}, {@link Disruptor#produceClaimAvail|produceClaimAvail} or {@link Disruptor#produceClaimAvailSync|produceClaimAvailSync}, minus 1.
     */
    get prevClaimEnd()
    {
    }

    /**
      @returns {integer} - The Disruptor maintains a strictly increasing count of the total number of elements consumed since it was created. This is the how many elements were consumed _before_ the previous call to {@link Disruptor#consumeNew|consumeNew} or {@link Disruptor#consumeNewSync|consumeNewSync}.
     */
    get prevConsumeStart()
    {
    }

    /**
      @returns {integer} - Size of each element in the Disruptor in bytes.
     */
    get elementSize()
    {
    }

    /**
     @returns {boolean} - Whether methods on this object which read from the Disruptor won't return to your application until a value is ready.
     */
    get spin()
    {
    }
}

/**
  Callback type for reserving a single element in the Disruptor for writing.

  @param {?Error} err - Error, if one occurred.
  @param {Buffer} buf - Buffer for writing data to the Disruptor. If the Disruptor was full and `spin` (see the {@link Disruptor|constructor}) is `false`, `buf` will be empty. Otherwise its length will be `element_size`. `buf` is backed by shared memory so may be overwritten after you call {@link Disruptor#produceCommit|produceCommit} or {@link Disruptor#produceCommitSync|produceCommitSync}.
  @param {integer} claimStart - The Disruptor maintains a strictly increasing count of the total number of elements produced since it was created. This is how many elements were produced before `buf` was reserved.
  @param {integer} claimEnd - The Disruptor maintains a strictly increasing count of the total number of elements produced since it was created. This is how many elements were produced after `buf` was reserved, minus 1.
 */
function produceClaimCallback(err, buf, claimStart, claimEnd)
{
}

/**
  Callback type for reserving a number of elements in the Disruptor for writing.

  @param {?Error} err - Error, if one occurred.
  @param {Buffer[]} bufs - Array of buffers for writing data to the Disruptor. If the Disruptor didn't have enought free elements and `spin` (see the {@link Disruptor|constructor}) is `false`, `bufs` will be empty. Otherwise, it will contain at least one buffer and each buffer will be a multiple of `element_size` in length. The maximum length of all buffers will be `element_size * num_elements` bytes. The buffers are backed by shared memory so may be overwritten after you call {@link Disruptor#produceCommit|produceCommit} or {@link Disruptor#produceCommitSync|produceCommitSync}.
  @param {integer} claimStart - The Disruptor maintains a strictly increasing count of the total number of elements produced since it was created. This is how many elements were produced before `bufs` was reserved.
  @param {integer} claimEnd - The Disruptor maintains a strictly increasing count of the total number of elements produced since it was created. This is how many elements were produced after `bufs` was reserved, minus 1.
 */
function produceClaimManyCallback(err, bufs, claimStart, claimEnd)
{
}

/**
  Callback type for commiting data to the Disruptor

  @param {?Error} err - Error, if one occurred.
  @param {boolean} committed - Whether the data was committed to the Disruptor. If some elements reserved before `claimStart` remain uncommitted and `spin` (see the {@link Disruptor|constructor}) is `false`, `committed` will be `false`. Otherwise the data was committed and `committed` will be `true`.
 */
function produceCommitCallback(err, committed)
{
}

/**
  Callback type for reading new data from the Disruptor

  @param {?Error} err - Error, if one occurred.
  @param {Buffer[]} bufs - Array of buffers containing new data ready to read from the Disruptor. If no new data was available and `spin` (see the {@link Disruptor|constructor}) is `false`, the array will be empty. Otherwise it will contain at least one buffer and each buffer will be a multiple of `element_size` in length. The buffers are backed by shared memory so may be overwritten after you call {@link Disruptor#consumeCommit|consumeCommit}.
  @param {integer} start - The Disruptor maintains a strictly increasing count of the total number of elements consumed since it was created. This is how many elements were consumed before `bufs` was read (if `bufs` isn't empty).
 */
function consumeNewCallback(err, bufs, start)
{
}

const stream = require('stream');

/**
  Creates a stream which reads from a disruptor.

  @param {Disruptor} disruptor - {@link Disruptor} to read from. Its `element_size` must be 1 byte and it must not `spin`.
  @param {Object} options - Passed to {@link stream.Readable}.
 */
class DisruptorReadStream extends stream.Readable
{
    constructor(disruptor, options)
    {
    }
}

/**
  Creates a stream which writes to a disruptor.

  @param {Disruptor} disruptor - {@link Distruptor} to write to. Its `element_size` must be 1 byte and it must not `spin`.
  @param {Object} options - Passed to {@link stream.Writable}.
 */
class DisruptorWriteStream extends stream.Writable
{
    constructor(disruptor, options)
    {
    }
}
