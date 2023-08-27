#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <memory>
#include <napi.h>
#include <memory>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <limits>

typedef uint64_t sequence_t;
typedef int32_t status_t;

const sequence_t sequence_max = std::numeric_limits<sequence_t>::max();

// Needs to be heap allocated because we access it from finalizers which can be called
// on process exit
static std::unordered_set<uint8_t*> *buffers;
static std::mutex buffers_mutex;

const int memorder = __ATOMIC_SEQ_CST;

class Disruptor : public Napi::ObjectWrap<Disruptor>
{
public:
    Disruptor(const Napi::CallbackInfo& info);
    ~Disruptor();

    static Napi::Object Initialize(Napi::Env env, Napi::Object exports);

    // Unmap the shared memory. Don't access it again from this Disruptor!
    void Release(const Napi::CallbackInfo& info);

    // Return unconsumed slots for a consumer
    Napi::Value ConsumeNew(const Napi::CallbackInfo& info); 
    Napi::Value ConsumeNewSync(const Napi::CallbackInfo& info); 

    // Commit consumed slots
    Napi::Value ConsumeCommit(const Napi::CallbackInfo&);

    // Claim a slot for writing a value
    Napi::Value ProduceClaim(const Napi::CallbackInfo& info);
    Napi::Value ProduceClaimSync(const Napi::CallbackInfo& info);

    // Claim multiple slots for writing values
    Napi::Value ProduceClaimMany(const Napi::CallbackInfo& info);
    Napi::Value ProduceClaimManySync(const Napi::CallbackInfo& info);

    // Claim all available slots for writing values
    Napi::Value ProduceClaimAvail(const Napi::CallbackInfo& info);
    Napi::Value ProduceClaimAvailSync(const Napi::CallbackInfo& info);

    // Commit a claimed slot
    Napi::Value ProduceCommit(const Napi::CallbackInfo& info);
    Napi::Value ProduceCommitSync(const Napi::CallbackInfo& info);

    // Get slots previously claimed but not committed
    Napi::Value ProduceRecover(const Napi::CallbackInfo& info);

    // Get size of each element in bytes
    Napi::Value GetElementSize(const Napi::CallbackInfo& info);

    // Get whether not to return while not ready or full
    Napi::Value GetSpin(const Napi::CallbackInfo& info);

    // Get status value
    Napi::Value GetStatus(const Napi::CallbackInfo& info);

    // Set status value
    void SetStatus(const Napi::CallbackInfo& info, const Napi::Value& value);

    inline bool Spin()
    {
        return spin;
    }

private:
    friend class ConsumeNewAsyncWorker;
    friend class ProduceClaimAsyncWorker;
    friend class ProduceClaimManyAsyncWorker;
    friend class ProduceClaimAvailAsyncWorker;
    friend class ProduceCommitAsyncWorker;
    friend class SyncBuffer;
    friend class AsyncBuffer;

    int Release();

    void UpdatePending(sequence_t seq_consumer, sequence_t seq_cursor);
    void UpdateSeqNext(const sequence_t seq_next,
                       const sequence_t seq_next_end,
                       const bool all_ignored);
    uint32_t GetSeqNext(const Napi::CallbackInfo& info,
                        sequence_t& seq_next,
                        sequence_t& seq_next_end);

    template<typename Array, typename DisruptorBuffer>
    void ProduceGetBuffers(const Napi::Env& env,
                           const sequence_t seq_next,
                           const sequence_t seq_next_end,
                           const bool all_ignored,
                           Array& r);

    template<typename Array, typename DisruptorBuffer>
    Array ConsumeNewSync(const Napi::Env& env, const bool retry, sequence_t& start);
    void ConsumeNewAsync(const Napi::CallbackInfo& info); 

    bool ConsumeCommit();

    template<typename DisruptorBuffer>
    typename DisruptorBuffer::Buffer ProduceClaimSync(const Napi::Env& env,
                                                      const bool retry,
                                                      sequence_t& out_next,
                                                      sequence_t& out_next_end,
                                                      bool& out_all_ignored);
    void ProduceClaimAsync(const Napi::CallbackInfo& info);

    template<typename Array, typename DisruptorBuffer>
    Array ProduceClaimManySync(const Napi::Env& env,
                               const uint32_t n,
                               const bool retry,
                               sequence_t& out_next,
                               sequence_t& out_next_end,
                               bool& out_all_ignored);
    void ProduceClaimManyAsync(const Napi::CallbackInfo& info);
    void ProduceClaimManyAsync(const Napi::CallbackInfo& info,
                               const uint32_t n);

    template<typename Array, typename DisruptorBuffer>
    Array ProduceClaimAvailSync(const Napi::Env& env,
                                const uint32_t max,
                                const bool retry,
                                sequence_t& out_next,
                                sequence_t& out_next_end,
                                bool& out_all_ignored);
    void ProduceClaimAvailAsync(const Napi::CallbackInfo& info);
    void ProduceClaimAvailAsync(const Napi::CallbackInfo& info,
                                const uint32_t max);

    template<typename Boolean>
    Boolean ProduceCommitSync(const Napi::Env& env,
                              sequence_t seq_next,
                              sequence_t seq_next_end,
                              const bool retry);
    void ProduceCommitAsync(const Napi::CallbackInfo& info);
    void ProduceCommitAsync(const Napi::CallbackInfo& info,
                            sequence_t seq_next,
                            sequence_t seq_next_end,
                            const uint32_t cb_arg);

    uint32_t num_elements;
    uint32_t element_size;
    uint32_t num_consumers;
    uint32_t consumer;
    bool init;
    bool spin;

    size_t shm_size;
    void* shm_buf;
    
    sequence_t *consumers; // for each consumer, next slot to read
    sequence_t *cursor;    // next slot to be filled
    sequence_t *next;      // next slot to claim
    status_t *status;     // status code (app-specific)
    uint8_t* elements;
    sequence_t *ptr_consumer;

    sequence_t pending_seq_consumer;
    sequence_t pending_seq_cursor;

    sequence_t pending_seq_next;
    sequence_t pending_seq_next_end;

    bool all_consumers_ignoring;

    Napi::Reference<Napi::Buffer<uint8_t>> shm_buffer_ref;
    Napi::Reference<Napi::Buffer<uint8_t>> elements_buffer_ref;
    Napi::Reference<Napi::Buffer<uint8_t>> consumers_buffer_ref;
    Napi::FunctionReference slice_ref;

    Napi::Value GetConsumers(const Napi::CallbackInfo& info);
    Napi::Value GetCursor(const Napi::CallbackInfo& info);
    Napi::Value GetNext(const Napi::CallbackInfo& info);
    Napi::Value GetElements(const Napi::CallbackInfo& info);
    Napi::Value GetConsumer(const Napi::CallbackInfo& info);
    Napi::Value GetPendingSeqConsumer(const Napi::CallbackInfo& info);
    Napi::Value GetPendingSeqCursor(const Napi::CallbackInfo& info);
    Napi::Value GetPendingSeqNext(const Napi::CallbackInfo& info);
    Napi::Value GetPendingSeqNextEnd(const Napi::CallbackInfo& info);
    Napi::Value GetAllConsumersIgnoring(const Napi::CallbackInfo& info);

    void ThrowErrnoError(const Napi::CallbackInfo& info,
                         const char *msg);
};

//LCOV_EXCL_START
void NullCallback(const Napi::CallbackInfo&)
{
}
//LCOV_EXCL_STOP

Napi::Function GetCallback(const Napi::CallbackInfo& info, const uint32_t cb_arg)
{
    if (info.Length() > cb_arg)
    {
        Napi::Value cb = info[cb_arg];
        if (cb.IsFunction())
        {
            return cb.As<Napi::Function>();
        }
    }

    return Napi::Function::New<&NullCallback>(info.Env()); //LCOV_EXCL_LINE
}

class SyncBuffer
{
public:
    typedef Napi::Buffer<uint8_t> Buffer;

    static Buffer New(Napi::Env env, Disruptor *d, sequence_t start, sequence_t end)
    {
#ifdef COVERAGE
        return d->slice_ref.Value().Call(d->elements_buffer_ref.Value(),
#else
        return d->slice_ref.Call(d->elements_buffer_ref.Value(),
#endif
        {
            Napi::Number::New(env, start * d->element_size),
            Napi::Number::New(env, end * d->element_size)
        }).As<Buffer>();
    }
};

class AsyncBuffer
{
public:
    typedef AsyncBuffer Buffer;

    AsyncBuffer() : AsyncBuffer(0, 0, 0)
    {
    }

    static Buffer New(Napi::Env, Disruptor *d, sequence_t start, sequence_t end)
    {
        return Buffer(start, end, d->element_size);
    }

    Napi::Value ToValue(Napi::Env env, Disruptor *d)
    {
        return SyncBuffer::New(env, d, start, end);
    }

    size_t Length()
    {
        return length;
    }

private:
    AsyncBuffer(sequence_t start, sequence_t end, uint32_t element_size) :
        start(start),
        end(end),
        length((end - start) * element_size)
    {
    }

    sequence_t start;
    sequence_t end;
    size_t length;
};

template<typename T>
class AsyncArray
{
public:
    AsyncArray() :
        elements(std::make_unique<std::vector<T>>())
    {
    }

    static AsyncArray<T> New(Napi::Env)
    {
        return AsyncArray<T>();
    }

    void Set(uint32_t index, T&& el)
    {
        if (elements->size() <= index)
        {
            elements->resize(index + 1);
        }

        (*elements)[index] = std::move(el);
    }

    Napi::Value ToValue(Napi::Env env, Disruptor *d)
    {
        size_t length = elements->size();
        Napi::Array r = Napi::Array::New(env);
        for (size_t i = 0; i < length; ++i)
        {
            r[i] = (*elements)[i].ToValue(env, d);
        }
        return r;
    }

    size_t Length()
    {
        return elements->size();
    }

private:
    std::unique_ptr<std::vector<T>> elements;
};

class AsyncBoolean
{
public:
    AsyncBoolean() :
        b(false)
    {
    }

    static AsyncBoolean New(Napi::Env, bool b)
    {
        return AsyncBoolean(b);
    }

    Napi::Value ToValue(Napi::Env env, Disruptor*)
    {
        return Napi::Boolean::New(env, b);
    }

    operator bool() const
    {
        return b;
    }

private:
    AsyncBoolean(bool b) :
        b(b)
    {
    }

    bool b;
};

class AsyncUndefined
{
};

Napi::Number ToValue(const Napi::Env& env, sequence_t n)
{
    return Napi::Number::New(env, n);
}

Napi::Value ToValue(const Napi::Env& env, const AsyncUndefined&)
{
    return env.Undefined();
}

template <typename Result,
          typename Arg1 = AsyncUndefined,
          typename Arg2 = AsyncUndefined,
          typename Arg3 = AsyncUndefined>
class DisruptorAsyncWorker : public Napi::AsyncWorker
{
public:
    DisruptorAsyncWorker(Disruptor *disruptor,
                         const Napi::Function& callback) :
        Napi::AsyncWorker(callback),
        retry(false),
        disruptor(disruptor), // disruptor_ref keeps this around
        disruptor_ref(Napi::Persistent(disruptor->Value()))
    {
    }

protected:
    virtual void Retry() = 0;

    void OnOK() override
    {
        if (disruptor->Spin() && retry)
        {
            return Retry();
        }

        Napi::Env env = Env();

        Callback().MakeCallback(
            Receiver().Value(),
            std::initializer_list<napi_value>
            {
                env.Null(),
                result.ToValue(env, disruptor),
                ToValue(env, arg1),
                ToValue(env, arg2),
                ToValue(env, arg3)
            });
    }

    Result result;
    Arg1 arg1;
    Arg2 arg2;
    Arg3 arg3;
    bool retry;
    Disruptor *disruptor;

private:
    Napi::ObjectReference disruptor_ref;
};

class CloseFD
{
public:
    void operator()(int *fd)
    {
        close(*fd);
        delete fd;
    }
};

void Disruptor::ThrowErrnoError(const Napi::CallbackInfo& info,
                                const char *msg)
{
    int errnum = errno;
    char buf[1024] = {0};
    auto err = strerror_r(errnum, buf, sizeof(buf));

    char *errmsg;
    if (std::is_same<decltype(err), int>::value) { // Use value to check type
        errmsg = (err == 0) ? buf : nullptr;
    }
    throw Napi::Error::New(info.Env(),
        std::string(msg) + ": " + (errmsg ? errmsg : std::to_string(errnum)));
}

Disruptor::Disruptor(const Napi::CallbackInfo& info) :
    Napi::ObjectWrap<Disruptor>(info),
    shm_buf(MAP_FAILED)
{
    // Arguments
    Napi::String shm_name = info[0].As<Napi::String>();
    num_elements = info[1].As<Napi::Number>();
    element_size = info[2].As<Napi::Number>();
    num_consumers = info[3].As<Napi::Number>();
    consumer = info[4].As<Napi::Number>();
    init = info[5].As<Napi::Boolean>();
    spin = info[6].As<Napi::Boolean>();

    // Open shared memory object
    // OS X does not allow using O_TRUNC with shm_open.
    // If this item exists, and init flag is true, delete it and recreate.
    const auto name = shm_name.Utf8Value();
    int shm_fd_tmp = shm_open(name.c_str(),
        (init ? O_CREAT | O_EXCL : 0) | O_RDWR,
        S_IRUSR | S_IWUSR);

    if (init && shm_fd_tmp < 0 && errno == EEXIST)
    {
        shm_unlink(name.c_str());
        shm_fd_tmp = shm_open(name.c_str(),
            O_CREAT | O_EXCL | O_RDWR,
            S_IRUSR | S_IWUSR);
    }

    if (shm_fd_tmp < 0)
    {
        ThrowErrnoError(info, "Failed to open shared memory object");
    }

    std::unique_ptr<int, CloseFD> shm_fd(new int(shm_fd_tmp));

    // Allow space for:
    // - a sequence number for each consumer
    // - the cursor sequence number (last filled slot)
    // - the next sequence number (first free slot)
    // - a status code
    // - all the elements
    shm_size = (num_consumers + 2) * sizeof(sequence_t) +
               sizeof(status_t) +
               num_elements * element_size;

    // Resize the shared memory if we're initializing it.
    // Note: ftruncate initializes to null bytes.
    if (init && (ftruncate(*shm_fd, shm_size) < 0))
    {
        ThrowErrnoError(info, "Failed to size shared memory"); //LCOV_EXCL_LINE
    }

    // Map the shared memory
    shm_buf = mmap(NULL,
                   shm_size,
                   PROT_READ | PROT_WRITE, MAP_SHARED,
                   *shm_fd,
                   0);
    if (shm_buf == MAP_FAILED)
    {
        ThrowErrnoError(info, "Failed to map shared memory"); //LCOV_EXCL_LINE
    }

    consumers = static_cast<sequence_t*>(shm_buf);
    cursor = &consumers[num_consumers];
    next = &cursor[1];
    status = reinterpret_cast<status_t*>(&next[1]);
    elements = reinterpret_cast<uint8_t*>(&status[1]);
    ptr_consumer = &consumers[consumer];

    pending_seq_consumer = 0;
    pending_seq_cursor = 0;

    pending_seq_next = 1;
    pending_seq_next_end = 0;

    all_consumers_ignoring = false;

    // From Node 14, V8 doesn't allow buffers pointing to the same memory:
    //
    // https://monorail-prod.appspot.com/p/v8/issues/detail?id=9908
    // https://github.com/nodejs/node/issues/32463
    //
    // We work around this by having a single Buffer over all of shm_buf and using
    // Buffer#slice() to return data from it.
    //
    // This leaves the issue of shm_buf getting the same address as a previous call -
    // whether because the same shared memory is mapped or because it's been
    // unmapped in Release() and then remapped again.
    // 
    // Since Node 14.3.0, we can be sure that the finalizer is run after the memory
    // pointer is removed from the BackingStore:
    //
    // https://github.com/nodejs/node/pull/33321
    //
    // However, if the memory is Release()d before it's removed from the BackingStore
    // then mmap may return it again and we won't be able to pass it to V8 without
    // it exiting.
    //
    // Further, even if we remember the Buffers we create in an unordered_map, they
    // become invalid before the finalizer is called. The finalizer is only guaranteed
    // to be called sometime after the object is collected:
    //
    // https://github.com/nodejs/node-addon-api/issues/702#issuecomment-625897608
    //
    // This also applies to weak N-API references to the object.
    //
    // We can detect this using napi_get_reference_value but there's nothing we can
    // do about it since the value will still be in the BackingStore until the finalizer
    // is run. It's this window of time that's the problem - between the Buffer being
    // collected and the finalizer being called.
    //
    // The solution implemented below is to maintain an unordered set of shm_bufs that
    // are still alive in the BackingStore. If mmap returns one of these, we search
    // downwards for the next address not in the set, adjusting the length of the
    // Buffer we need to create accordingly. Since we're slicing Buffer views over it,
    // where it starts from doesn't matter.

    Napi::Env env = info.Env();
    const auto JSBuffer = env.Global().Get("Buffer").As<Napi::Function>();
    const auto proto = JSBuffer.Get("prototype").As<Napi::Object>();
    slice_ref = Napi::Persistent(proto.Get("slice").As<Napi::Function>());

    auto shm_buf8 = static_cast<uint8_t*>(shm_buf);
    auto shm_size8 = shm_size;
    Napi::Buffer<uint8_t> shm_buffer;

    {
        std::lock_guard<std::mutex> lock(buffers_mutex);

        while (shm_buf8) {
            if (buffers->find(shm_buf8) == buffers->end()) {
                break;
            }
            --shm_buf8;
            ++shm_size8;
        }
    }

    if (!shm_buf8) {
        //LCOV_EXCL_START
        Release();
        throw Napi::Error::New(env, "No space for buffer due to due to https://github.com/nodejs/node/issues/32463");
        //LCOV_EXCL_STOP
    }

    shm_buffer = Napi::Buffer<uint8_t>::New(
        env,
        shm_buf8,
        shm_size8,
        [](Napi::Env, uint8_t* shm_buf8) {
            std::lock_guard<std::mutex> lock(buffers_mutex);
            buffers->erase(shm_buf8);
        });

    {
        std::lock_guard<std::mutex> lock(buffers_mutex);
        buffers->emplace(shm_buf8);
    }

    shm_buffer_ref = Napi::Persistent(shm_buffer);

    const auto elements_start = elements - shm_buf8;
    elements_buffer_ref = Napi::Persistent(slice_ref.Call(shm_buffer_ref.Value(),
    {
        Napi::Number::New(env, elements_start),
        Napi::Number::New(env, elements_start + num_elements * element_size)
    }).As<Napi::Buffer<uint8_t>>());

    const auto consumers_start = reinterpret_cast<uint8_t*>(consumers) - shm_buf8;
    consumers_buffer_ref = Napi::Persistent(slice_ref.Call(shm_buffer_ref.Value(),
    {
        Napi::Number::New(env, consumers_start),
        Napi::Number::New(env, consumers_start + num_consumers * sizeof(sequence_t))
    }).As<Napi::Buffer<uint8_t>>());
}

Disruptor::~Disruptor()
{
    Release();
}

int Disruptor::Release()
{
    shm_buffer_ref.Reset();
    elements_buffer_ref.Reset();
    consumers_buffer_ref.Reset();
    slice_ref.Reset();

    if (shm_buf != MAP_FAILED)
    {
        int r = munmap(shm_buf, shm_size);

        if (r < 0)
        {
            return r; //LCOV_EXCL_LINE
        }

        shm_buf = MAP_FAILED;
    }

    return 0;
}

void Disruptor::Release(const Napi::CallbackInfo& info)
{
    if ((shm_buf != MAP_FAILED) &&
        (info.Length() >= 1) &&
        info[0].As<Napi::Boolean>())
    {
        __atomic_store_n(ptr_consumer, sequence_max, memorder);
    }

    if (Release() < 0)
    {
        ThrowErrnoError(info, "Failed to unmap shared memory"); //LCOV_EXCL_LINE
    }
}

#include <iostream>

template<typename Array, typename DisruptorBuffer>
Array Disruptor::ConsumeNewSync(const Napi::Env& env,
                                const bool retry,
                                sequence_t &start)
{
    // Return all elements [&consumers[consumer], cursor)

    // Commit previous consume
    ConsumeCommit();

    do
    {
        sequence_t seq_consumer = __atomic_load_n(ptr_consumer, memorder);
        sequence_t seq_cursor = __atomic_load_n(cursor, memorder);
        sequence_t pos_consumer = seq_consumer % num_elements;
        sequence_t pos_cursor = seq_cursor % num_elements;

        if (pos_cursor > pos_consumer)
        {
            Array r = Array::New(env);
            r.Set(0U, DisruptorBuffer::New(env, this, pos_consumer, pos_cursor));
            UpdatePending(seq_consumer, seq_cursor);
            start = seq_consumer;
            return r;
        }

        if (seq_cursor != seq_consumer)
        {
            Array r = Array::New(env);
            r.Set(0U, DisruptorBuffer::New(env, this, pos_consumer, num_elements));
            if (pos_cursor > 0)
            {
                r.Set(1U, DisruptorBuffer::New(env, this, 0, pos_cursor));
            }
            UpdatePending(seq_consumer, seq_cursor);
            start = seq_consumer;
            return r;
        }
    }
    while (retry);

    start = 0;
    // ConsumeCommit() above already set pending_set_cursor to 0
    return Array::New(env);
}

Napi::Value Disruptor::ConsumeNewSync(const Napi::CallbackInfo& info)
{
    sequence_t start;
    return ConsumeNewSync<Napi::Array, SyncBuffer>(info.Env(), spin, start);
}

class ConsumeNewAsyncWorker :
    public DisruptorAsyncWorker<AsyncArray<AsyncBuffer>, sequence_t>
{
public:
    ConsumeNewAsyncWorker(Disruptor *disruptor,
                          const Napi::Function& callback) :
        DisruptorAsyncWorker<AsyncArray<AsyncBuffer>, sequence_t>(
            disruptor, callback)
    {
        arg1 = 0;
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ConsumeNewSync<AsyncArray<AsyncBuffer>, AsyncBuffer>(Env(), false, arg1);
        retry = result.Length() == 0;
    }

    void Retry() override
    {
        (new ConsumeNewAsyncWorker(disruptor, Callback().Value()))->Queue();
    }
};

void Disruptor::ConsumeNewAsync(const Napi::CallbackInfo& info)
{
    (new ConsumeNewAsyncWorker(this, GetCallback(info, 0)))->Queue();
}

Napi::Value Disruptor::ConsumeNew(const Napi::CallbackInfo& info)
{
    sequence_t start;
    Napi::Array r = ConsumeNewSync<Napi::Array, SyncBuffer>(
        info.Env(), false, start);

    if ((r.Length() > 0) || !spin)
    {
        return r;
    }

    ConsumeNewAsync(info);
    return info.Env().Undefined();
}

Napi::Value Disruptor::ConsumeCommit(const Napi::CallbackInfo& info)
{
    return Napi::Boolean::New(info.Env(), ConsumeCommit());
}

void Disruptor::UpdatePending(sequence_t seq_consumer, sequence_t seq_cursor)
{
    pending_seq_consumer = seq_consumer;
    pending_seq_cursor = seq_cursor;
}

bool Disruptor::ConsumeCommit()
{
    bool r = true;

    if (pending_seq_cursor)
    {
        sequence_t expected = pending_seq_consumer;
        r = __atomic_compare_exchange_n(ptr_consumer,
                                        &expected,
                                        pending_seq_cursor,
                                        false,
                                        memorder,
                                        memorder);
        pending_seq_cursor = 0;
    }

    return r;
}

template<typename DisruptorBuffer>
typename DisruptorBuffer::Buffer Disruptor::ProduceClaimSync(const Napi::Env& env,
                                                             const bool retry,
                                                             sequence_t& out_next,
                                                             sequence_t& out_next_end,
                                                             bool& out_all_ignored)
{
    bool all_ignored;

    do
    {
        sequence_t seq_next = __atomic_load_n(next, memorder);

        bool can_claim = true;
        all_ignored = true;

        for (uint32_t i = 0; i < num_consumers; ++i)
        {
            sequence_t seq_consumer = __atomic_load_n(&consumers[i], memorder);

            if (seq_consumer != sequence_max)
            {
                all_ignored = false;

                if ((seq_next - seq_consumer) >= num_elements)
                {
                    can_claim = false;
                    break;
                }
            }
        }
        
        if (all_ignored)
        {
            break;
        }

        if (can_claim &&
            __atomic_compare_exchange_n(next, &seq_next, seq_next + 1, false, memorder, memorder))
        {
            sequence_t start = seq_next % num_elements;
            auto r = DisruptorBuffer::New(env, this, start, start + 1);
            UpdateSeqNext(seq_next, seq_next, all_ignored);
            out_next = seq_next;
            out_next_end = seq_next;
            out_all_ignored = all_ignored;
            return r;
        }
    }
    while (retry);

    UpdateSeqNext(1, 0, all_ignored);
    out_next = 1;
    out_next_end = 0;
    out_all_ignored = all_ignored;
    return DisruptorBuffer::New(env, this, 0, 0);
}

Napi::Value Disruptor::ProduceClaimSync(const Napi::CallbackInfo& info)
{
    sequence_t seq_next, seq_next_end;
    bool all_ignored;
    return ProduceClaimSync<SyncBuffer>(info.Env(), spin, seq_next, seq_next_end, all_ignored);
}

class ProduceClaimAsyncWorker :
    public DisruptorAsyncWorker<AsyncBuffer, sequence_t, sequence_t, bool>
{
public:
    ProduceClaimAsyncWorker(Disruptor *disruptor,
                            const Napi::Function& callback) :
        DisruptorAsyncWorker<AsyncBuffer, sequence_t, sequence_t, bool>(
            disruptor, callback)
    {
        arg1 = 1;
        arg2 = 0;
        arg3 = false;
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ProduceClaimSync<AsyncBuffer>(
            Env(), false, arg1, arg2, arg3);
        retry = result.Length() == 0;
    }

    void Retry() override
    {
        (new ProduceClaimAsyncWorker(disruptor, Callback().Value()))->Queue();
    }
};

void Disruptor::ProduceClaimAsync(const Napi::CallbackInfo& info)
{
    (new ProduceClaimAsyncWorker(this, GetCallback(info, 0)))->Queue();
}

Napi::Value Disruptor::ProduceClaim(const Napi::CallbackInfo& info)
{
    sequence_t seq_next, seq_next_end;
    bool all_ignored;
    Napi::Buffer<uint8_t> r = ProduceClaimSync<SyncBuffer>(
        info.Env(), false, seq_next, seq_next_end, all_ignored);

    if ((r.Length() > 0) || all_ignored || !spin)
    {
        return r;
    }

    ProduceClaimAsync(info);
    return info.Env().Undefined();
}

template<typename Array, typename DisruptorBuffer>
void Disruptor::ProduceGetBuffers(const Napi::Env& env,
                                  const sequence_t seq_next,
                                  const sequence_t seq_next_end,
                                  const bool all_ignored,
                                  Array& r)
{
    sequence_t pos_next = seq_next % num_elements;
    sequence_t pos_next_end = seq_next_end % num_elements;

    if (pos_next_end < pos_next)
    {
        r.Set(0U, DisruptorBuffer::New(env, this, pos_next, num_elements));
        r.Set(1U, DisruptorBuffer::New(env, this, 0, pos_next_end + 1));
    }
    else
    {
        r.Set(0U, DisruptorBuffer::New(env, this, pos_next, pos_next_end + 1));
    }

    UpdateSeqNext(seq_next, seq_next_end, all_ignored);
}

template<typename Array, typename DisruptorBuffer>
Array Disruptor::ProduceClaimManySync(const Napi::Env& env,
                                      const uint32_t n,
                                      const bool retry,
                                      sequence_t& out_next,
                                      sequence_t& out_next_end,
                                      bool& out_all_ignored)
{
    bool all_ignored;

    do
    {
        sequence_t seq_next = __atomic_load_n(next, memorder);
        sequence_t seq_next_end = seq_next + std::min(n, num_elements) - 1;

        bool can_claim = true;
        all_ignored = true;

        for (uint32_t i = 0; i < num_consumers; ++i)
        {
            sequence_t seq_consumer = __atomic_load_n(&consumers[i], memorder);

            if (seq_consumer != sequence_max)
            {
                all_ignored = false;                

                if ((seq_next_end - seq_consumer) >= num_elements)
                {
                    can_claim = false;
                    break;
                }
            }
        }

        if (all_ignored)
        {
            break;
        }

        if (can_claim &&
            __atomic_compare_exchange_n(next, &seq_next, seq_next_end + 1, false, memorder, memorder))
        {
            Array r = Array::New(env);
            ProduceGetBuffers<Array, DisruptorBuffer>(env, seq_next, seq_next_end, all_ignored, r);
            out_next = seq_next;
            out_next_end = seq_next_end;
            out_all_ignored = all_ignored;
            return r;
        }
    }
    while (retry);

    UpdateSeqNext(1, 0, all_ignored);
    out_next = 1;
    out_next_end = 0;
    out_all_ignored = all_ignored;
    return Array::New(env);
}

Napi::Value Disruptor::ProduceClaimManySync(const Napi::CallbackInfo& info)
{
    sequence_t seq_next, seq_next_end;
    bool all_ignored;
    return ProduceClaimManySync<Napi::Array, SyncBuffer>(
        info.Env(), info[0].As<Napi::Number>(), spin, seq_next, seq_next_end, all_ignored);
}

class ProduceClaimManyAsyncWorker :
    public DisruptorAsyncWorker<AsyncArray<AsyncBuffer>,
                                sequence_t,
                                sequence_t,
                                bool>
{
public:
    ProduceClaimManyAsyncWorker(Disruptor *disruptor,
                                const Napi::Function& callback,
                                uint32_t n) :
        DisruptorAsyncWorker<AsyncArray<AsyncBuffer>,
                             sequence_t,
                             sequence_t,
                             bool>(
            disruptor, callback),
        n(n)
    {
        arg1 = 1;
        arg2 = 0;
        arg3 = false;
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ProduceClaimManySync<AsyncArray<AsyncBuffer>, AsyncBuffer>(
            Env(), n, false, arg1, arg2, arg3);
        retry = result.Length() == 0;
    }

    void Retry() override
    {
        (new ProduceClaimManyAsyncWorker(disruptor, Callback().Value(), n))->Queue();
    }

private:
    uint32_t n;
};

void Disruptor::ProduceClaimManyAsync(const Napi::CallbackInfo& info,
                                      uint32_t n)
{
    (new ProduceClaimManyAsyncWorker(this, GetCallback(info, 1), n))->Queue();
}

void Disruptor::ProduceClaimManyAsync(const Napi::CallbackInfo& info)
{
    ProduceClaimManyAsync(info, info[0].As<Napi::Number>());
}

Napi::Value Disruptor::ProduceClaimMany(const Napi::CallbackInfo& info)
{
    uint32_t n = info[0].As<Napi::Number>();
    sequence_t seq_next, seq_next_end;
    bool all_ignored;
    Napi::Array r = ProduceClaimManySync<Napi::Array, SyncBuffer>(
        info.Env(), n, false, seq_next, seq_next_end, all_ignored);

    if ((r.Length() > 0) || all_ignored || !spin)
    {
        return r;
    }

    ProduceClaimManyAsync(info, n);
    return info.Env().Undefined();
}

template<typename Array, typename DisruptorBuffer>
Array Disruptor::ProduceClaimAvailSync(const Napi::Env& env,
                                       const uint32_t max,
                                       const bool retry,
                                       sequence_t& out_next,
                                       sequence_t& out_next_end,
                                       bool& out_all_ignored)
{
    bool all_ignored;

    do
    {
        sequence_t seq_next = __atomic_load_n(next, memorder);
        auto n = std::min(max, num_elements);
        all_ignored = true;

        for (uint32_t i = 0; i < num_consumers; ++i)
        {
            sequence_t seq_consumer = __atomic_load_n(&consumers[i], memorder);
            if (seq_consumer != sequence_max)
            {
                all_ignored = false;
                n = std::min(static_cast<sequence_t>(n), num_elements - (seq_next - seq_consumer));
            }
        }

        if (all_ignored)
        {
            break;
        }

        if ((n > 0) &&
            __atomic_compare_exchange_n(next, &seq_next, seq_next + n, false, memorder, memorder))
        {
            Array r = Array::New(env);
            ProduceGetBuffers<Array, DisruptorBuffer>(env, seq_next, seq_next + n - 1, all_ignored, r);
            out_next = seq_next;
            out_next_end = seq_next + n - 1;
            out_all_ignored = all_ignored;
            return r;
        }
    }
    while (retry);

    UpdateSeqNext(1, 0, all_ignored);
    out_next = 1;
    out_next_end = 0;
    out_all_ignored = all_ignored;
    return Array::New(env);
}

Napi::Value Disruptor::ProduceClaimAvailSync(const Napi::CallbackInfo& info)
{
    sequence_t seq_next, seq_next_end;
    bool all_ignored;
    return ProduceClaimAvailSync<Napi::Array, SyncBuffer>(
        info.Env(), info[0].As<Napi::Number>(), spin, seq_next, seq_next_end, all_ignored);
}

class ProduceClaimAvailAsyncWorker :
    public DisruptorAsyncWorker<AsyncArray<AsyncBuffer>,
                                sequence_t,
                                sequence_t,
                                bool>
{
public:
    ProduceClaimAvailAsyncWorker(Disruptor *disruptor,
                                 const Napi::Function& callback,
                                 uint32_t max) :
        DisruptorAsyncWorker<AsyncArray<AsyncBuffer>,
                             sequence_t,
                             sequence_t,
                             bool>(
            disruptor, callback),
        max(max)
    {
        arg1 = 1;
        arg2 = 0;
        arg3 = false;
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ProduceClaimAvailSync<AsyncArray<AsyncBuffer>, AsyncBuffer>(
            Env(), max, false, arg1, arg2, arg3);
        retry = result.Length() == 0;
    }

    void Retry() override
    {
        (new ProduceClaimAvailAsyncWorker(disruptor, Callback().Value(), max))->Queue();
    }

private:
    uint32_t max;
};

void Disruptor::ProduceClaimAvailAsync(const Napi::CallbackInfo& info,
                                       uint32_t max)
{
    (new ProduceClaimAvailAsyncWorker(this, GetCallback(info, 1), max))->Queue();
}


void Disruptor::ProduceClaimAvailAsync(const Napi::CallbackInfo& info)
{
    ProduceClaimAvailAsync(info, info[0].As<Napi::Number>());
}

Napi::Value Disruptor::ProduceClaimAvail(const Napi::CallbackInfo& info)
{
    uint32_t max = info[0].As<Napi::Number>();
    sequence_t seq_next, seq_next_end;
    bool all_ignored;
    Napi::Array r = ProduceClaimAvailSync<Napi::Array, SyncBuffer>(
        info.Env(), max, false, seq_next, seq_next_end, all_ignored);

    if ((r.Length() > 0) || all_ignored || !spin)
    {
        return r;
    }

    ProduceClaimAvailAsync(info, max);
    return info.Env().Undefined();
}

Napi::Value Disruptor::ProduceRecover(const Napi::CallbackInfo& info)
{
    sequence_t seq_next = info[0].As<Napi::Number>().Int64Value();
    sequence_t seq_next_end = info[1].As<Napi::Number>().Int64Value();

    Napi::Array r = Napi::Array::New(info.Env());

    if ((seq_next <= seq_next_end) &&
        (__atomic_load_n(cursor, memorder) <= seq_next) &&
        (__atomic_load_n(next, memorder) > seq_next_end))
    {
        ProduceGetBuffers<Napi::Array, SyncBuffer>(
            info.Env(), seq_next, seq_next_end, false, r);
    }

    return r;
}

template<typename Boolean>
Boolean Disruptor::ProduceCommitSync(const Napi::Env& env,
                                     sequence_t seq_next,
                                     sequence_t seq_next_end,
                                     const bool retry)
{
    if (seq_next <= seq_next_end)
    {
        do
        {
            sequence_t expected = seq_next;
            if (__atomic_compare_exchange_n(cursor, &expected, seq_next_end + 1, false, memorder, memorder))
            {
                return Boolean::New(env, true);
            }
        }
        while (retry);
    }

    return Boolean::New(env, false);
}

void Disruptor::UpdateSeqNext(const sequence_t seq_next,
                              const sequence_t seq_next_end,
                              const bool all_ignored)
{
    pending_seq_next = seq_next;
    pending_seq_next_end = seq_next_end;
    all_consumers_ignoring = all_ignored;
}

uint32_t Disruptor::GetSeqNext(const Napi::CallbackInfo& info,
                               sequence_t& seq_next,
                               sequence_t& seq_next_end)
{
    if (info.Length() >= 2)
    {
        seq_next = info[0].As<Napi::Number>().Int64Value();
        seq_next_end = info[1].As<Napi::Number>().Int64Value();
        return 2;
    }

    seq_next = pending_seq_next;
    seq_next_end = pending_seq_next_end;
    return 0;
}

Napi::Value Disruptor::ProduceCommitSync(const Napi::CallbackInfo& info)
{
    sequence_t seq_next, seq_next_end;
    GetSeqNext(info, seq_next, seq_next_end);
    return ProduceCommitSync<Napi::Boolean>(info.Env(), seq_next, seq_next_end, spin);
}

class ProduceCommitAsyncWorker :
    public DisruptorAsyncWorker<AsyncBoolean>
{
public:
    ProduceCommitAsyncWorker(Disruptor *disruptor,
                             const Napi::Function& callback,
                             sequence_t seq_next,
                             sequence_t seq_next_end) :
        DisruptorAsyncWorker<AsyncBoolean>(disruptor, callback),
        seq_next(seq_next),
        seq_next_end(seq_next_end)
    {
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ProduceCommitSync<AsyncBoolean>(Env(), seq_next, seq_next_end, false);
        retry = !result;
    }

    void Retry() override
    {
        (new ProduceCommitAsyncWorker(disruptor, Callback().Value(), seq_next, seq_next_end))->Queue();
    }

private:
    sequence_t seq_next, seq_next_end;
};

void Disruptor::ProduceCommitAsync(const Napi::CallbackInfo& info,
                                   sequence_t seq_next,
                                   sequence_t seq_next_end,
                                   uint32_t cb_arg)
{
    (new ProduceCommitAsyncWorker(
        this, GetCallback(info, cb_arg), seq_next, seq_next_end))->Queue();
}

void Disruptor::ProduceCommitAsync(const Napi::CallbackInfo& info)
{
    sequence_t seq_next, seq_next_end;
    uint32_t cb_arg = GetSeqNext(info, seq_next, seq_next_end);
    ProduceCommitAsync(info, seq_next, seq_next_end, cb_arg);
}

Napi::Value Disruptor::ProduceCommit(const Napi::CallbackInfo& info)
{
    sequence_t seq_next, seq_next_end;
    uint32_t cb_arg = GetSeqNext(info, seq_next, seq_next_end);

    Napi::Boolean r = ProduceCommitSync<Napi::Boolean>(info.Env(), seq_next, seq_next_end, false);
    if (r || !spin)
    {
        return r;
    }

    ProduceCommitAsync(info, seq_next, seq_next_end, cb_arg);
    return info.Env().Undefined();
}

Napi::Value Disruptor::GetConsumers(const Napi::CallbackInfo&)
{
    return consumers_buffer_ref.Value();
}

Napi::Value Disruptor::GetCursor(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __atomic_load_n(cursor, memorder));
}

Napi::Value Disruptor::GetNext(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __atomic_load_n(next, memorder));
}

Napi::Value Disruptor::GetElements(const Napi::CallbackInfo&)
{
    return elements_buffer_ref.Value();
}

Napi::Value Disruptor::GetConsumer(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __atomic_load_n(ptr_consumer, memorder));
}

Napi::Value Disruptor::GetPendingSeqConsumer(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), pending_seq_consumer);
}

Napi::Value Disruptor::GetPendingSeqCursor(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), pending_seq_cursor);
}

Napi::Value Disruptor::GetPendingSeqNext(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), pending_seq_next);
}

Napi::Value Disruptor::GetPendingSeqNextEnd(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), pending_seq_next_end);
}

Napi::Value Disruptor::GetAllConsumersIgnoring(const Napi::CallbackInfo& info)
{
    return Napi::Boolean::New(info.Env(), all_consumers_ignoring);
}

Napi::Value Disruptor::GetElementSize(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), element_size);
}

Napi::Value Disruptor::GetSpin(const Napi::CallbackInfo& info)
{
    return Napi::Boolean::New(info.Env(), spin);
}

Napi::Value Disruptor::GetStatus(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __atomic_load_n(status, memorder));
}

void Disruptor::SetStatus(const Napi::CallbackInfo&, const Napi::Value& value)
{
    __atomic_store_n(status, value.As<Napi::Number>(), memorder);
}

Napi::Object Disruptor::Initialize(Napi::Env env, Napi::Object exports)
{
    {
        std::lock_guard<std::mutex> lock(buffers_mutex);
        if (!buffers) {
            buffers = new std::unordered_set<uint8_t*>();
        }
    }

    exports.Set("Disruptor", DefineClass(env, "Disruptor",
    {
        InstanceMethod<&Disruptor::ProduceClaim>("produceClaim"),
        InstanceMethod<&Disruptor::ProduceClaimSync>("produceClaimSync"),
        InstanceMethod<&Disruptor::ProduceClaimMany>("produceClaimMany"),
        InstanceMethod<&Disruptor::ProduceClaimManySync>("produceClaimManySync"),
        InstanceMethod<&Disruptor::ProduceClaimAvail>("produceClaimAvail"),
        InstanceMethod<&Disruptor::ProduceClaimAvailSync>("produceClaimAvailSync"),
        InstanceMethod<&Disruptor::ProduceCommit>("produceCommit"),
        InstanceMethod<&Disruptor::ProduceCommitSync>("produceCommitSync"),
        InstanceMethod<&Disruptor::ProduceRecover>("produceRecover"),
        InstanceMethod<&Disruptor::ConsumeNew>("consumeNew"),
        InstanceMethod<&Disruptor::ConsumeNewSync>("consumeNewSync"),
        InstanceMethod<&Disruptor::ConsumeCommit>("consumeCommit"),
        InstanceMethod<&Disruptor::Release>("release"),
        InstanceAccessor<&Disruptor::GetPendingSeqConsumer>("prevConsumeStart"),
        InstanceAccessor<&Disruptor::GetPendingSeqNext>("prevClaimStart"),
        InstanceAccessor<&Disruptor::GetPendingSeqNextEnd>("prevClaimEnd"),
        InstanceAccessor<&Disruptor::GetAllConsumersIgnoring>("allConsumersIgnoring"),
        InstanceAccessor<&Disruptor::GetElementSize>("elementSize"),
        InstanceAccessor<&Disruptor::GetSpin>("spin"),
        InstanceAccessor<&Disruptor::GetStatus, &Disruptor::SetStatus>("status"),

        // For testing only
        InstanceAccessor<&Disruptor::GetConsumers>("consumers"),
        InstanceAccessor<&Disruptor::GetCursor>("cursor"),
        InstanceAccessor<&Disruptor::GetNext>("next"),
        InstanceAccessor<&Disruptor::GetElements>("elements"),
        InstanceAccessor<&Disruptor::GetConsumer>("consumer"),
        InstanceAccessor<&Disruptor::GetPendingSeqCursor>("prevConsumeNext"),
        InstanceMethod<&Disruptor::ConsumeNewAsync>("consumeNewAsync"),
        InstanceMethod<&Disruptor::ProduceClaimAsync>("produceClaimAsync"),
        InstanceMethod<&Disruptor::ProduceClaimManyAsync>("produceClaimManyAsync"),
        InstanceMethod<&Disruptor::ProduceClaimAvailAsync>("produceClaimAvailAsync"),
        InstanceMethod<&Disruptor::ProduceCommitAsync>("produceCommitAsync")
    }));

    return exports;
}

Napi::Object Initialize(Napi::Env env, Napi::Object exports)
{
    return Disruptor::Initialize(env, exports);
}

NODE_API_MODULE(disruptor, Initialize)
