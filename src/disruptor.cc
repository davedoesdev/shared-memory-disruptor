#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <memory>
#include <napi.h>
#include <memory>
#include <vector>
#include <unordered_map>

typedef uint64_t sequence_t;

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

    // Commit a claimed slot
    Napi::Value ProduceCommit(const Napi::CallbackInfo& info);
    Napi::Value ProduceCommitSync(const Napi::CallbackInfo& info);

    // Get slots previously claimed but not committed
    Napi::Value ProduceRecover(const Napi::CallbackInfo& info);

    inline bool Spin()
    {
        return spin;
    }

private:
    friend class ConsumeNewAsyncWorker;
    friend class ProduceClaimAsyncWorker;
    friend class ProduceClaimManyAsyncWorker;
    friend class ProduceCommitAsyncWorker;

    int Release();

    void UpdatePending(sequence_t seq_consumer, sequence_t seq_cursor);
    void UpdateSeqNext(sequence_t seq_next, sequence_t seq_next_end);
    uint32_t GetSeqNext(const Napi::CallbackInfo& info,
                        sequence_t& seq_next,
                        sequence_t& seq_next_end);

    template<typename Array, template<typename> typename Buffer>
    void ProduceGetBuffers(const Napi::Env& env,
                           sequence_t seq_next,
                           sequence_t seq_next_end,
                           Array& r);

    template<typename Array, template<typename> typename Buffer>
    Array ConsumeNewSync(const Napi::Env& env, bool retry, sequence_t& start);
    void ConsumeNewAsync(const Napi::CallbackInfo& info); 

    bool ConsumeCommit();

    template<template<typename> typename Buffer>
    Buffer<uint8_t> ProduceClaimSync(const Napi::Env& env,
                                     bool retry,
                                     sequence_t& out_next,
                                     sequence_t& out_next_end);
    void ProduceClaimAsync(const Napi::CallbackInfo& info);

    template<typename Array, template<typename> typename Buffer>
    Array ProduceClaimManySync(const Napi::Env& env,
                               uint32_t n,
                               bool retry,
                               sequence_t& out_next,
                               sequence_t& out_next_end);
    void ProduceClaimManyAsync(const Napi::CallbackInfo& info);
    void ProduceClaimManyAsync(const Napi::CallbackInfo& info,
                               uint32_t n);

    template<typename Boolean>
    Boolean ProduceCommitSync(const Napi::Env& env,
                              sequence_t seq_next,
                              sequence_t seq_next_end,
                              bool retry);
    void ProduceCommitAsync(const Napi::CallbackInfo& info);
    void ProduceCommitAsync(const Napi::CallbackInfo& info,
                            sequence_t seq_next,
                            sequence_t seq_next_end,
                            uint32_t cb_arg);

    uint32_t num_elements;
    uint32_t element_size;
    uint32_t num_consumers;
    uint32_t consumer;
    bool spin;

    size_t shm_size;
    void* shm_buf;
    
    sequence_t *consumers; // for each consumer, next slot to read
    sequence_t *cursor;    // next slot to be filled
    sequence_t *next;      // next slot to claim
    uint8_t* elements;
    sequence_t *ptr_consumer;

    sequence_t pending_seq_consumer;
    sequence_t pending_seq_cursor;

    sequence_t pending_seq_next;
    sequence_t pending_seq_next_end;

    Napi::Value GetConsumers(const Napi::CallbackInfo& info);
    Napi::Value GetCursor(const Napi::CallbackInfo& info);
    Napi::Value GetNext(const Napi::CallbackInfo& info);
    Napi::Value GetElements(const Napi::CallbackInfo& info);
    Napi::Value GetConsumer(const Napi::CallbackInfo& info);
    Napi::Value GetPendingSeqConsumer(const Napi::CallbackInfo& info);
    Napi::Value GetPendingSeqCursor(const Napi::CallbackInfo& info);
    Napi::Value GetPendingSeqNext(const Napi::CallbackInfo& info);
    Napi::Value GetPendingSeqNextEnd(const Napi::CallbackInfo& info);
    Napi::Value GetElementSize(const Napi::CallbackInfo& info);
};

void NullCallback(const Napi::CallbackInfo& info)
{
}

Napi::Function GetCallback(const Napi::CallbackInfo& info, uint32_t cb_arg)
{
    if (info.Length() > cb_arg)
    {
        Napi::Value cb = info[cb_arg];
        if (cb.IsFunction())
        {
            return cb.As<Napi::Function>();
        }
    }

    return Napi::Function::New(info.Env(), NullCallback);
}

template<typename T>
class AsyncBuffer
{
public:
    AsyncBuffer() :
        data(nullptr),
        length(0)
    {
    }

    static AsyncBuffer<T> New(Napi::Env, T* data, size_t length)
    {
        return AsyncBuffer<T>(data, length);
    }

    static AsyncBuffer<T> New(Napi::Env, size_t length)
    {
        return AsyncBuffer<T>(nullptr, length);
    }

    Napi::Value ToValue(Napi::Env env)
    {
        Napi::Buffer<T> r;

        if (data)
        {
            r = Napi::Buffer<T>::New(env, data, length);
        }
        else
        {
            r = Napi::Buffer<T>::New(env, length);
        }

        return r;
    }

    size_t Length()
    {
        return length;
    }

private:
    AsyncBuffer(T* data, size_t length) :
        data(data),
        length(length)
    {
    }

    T* data;
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

    void Set(uint32_t index, T el)
    {
        if (elements->size() <= index)
        {
            elements->resize(index + 1);
        }

        (*elements)[index] = std::move(el);
    }

    Napi::Value ToValue(Napi::Env env)
    {
        size_t length = elements->size();
        Napi::Array r = Napi::Array::New(env);
        for (size_t i = 0; i < length; ++i)
        {
            r[i] = (*elements)[i].ToValue(env);
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

    Napi::Value ToValue(Napi::Env env)
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

template <typename Result, typename Arg1, typename Arg2>
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
                result.ToValue(env),
                ToValue(env, arg1),
                ToValue(env, arg2)
            });
    }

    Result result;
    Arg1 arg1;
    Arg2 arg2;
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

void ThrowErrnoError(const Napi::CallbackInfo& info, const char *msg)
{
    int errnum = errno;
    char buf[1024] = {0};
    auto errmsg = strerror_r(errnum, buf, sizeof(buf));
    static_assert(std::is_same<decltype(errmsg), char*>::value,
                  "strerror_r must return char*");
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
    bool init = info[5].As<Napi::Boolean>();
    spin = info[6].As<Napi::Boolean>();

    // Open shared memory object
    std::unique_ptr<int, CloseFD> shm_fd(new int(
        shm_open(shm_name.Utf8Value().c_str(),
                 O_CREAT | O_RDWR | (init ? O_TRUNC : 0),
                 S_IRUSR | S_IWUSR)));
    if (*shm_fd < 0)
    {
        ThrowErrnoError(info, "Failed to open shared memory object");
    }

    // Allow space for all the elements,
    // a sequence number for each consumer,
    // the cursor sequence number (last filled slot) and
    // the next sequence number (first free slot).
    shm_size = (num_consumers + 2) * sizeof(sequence_t) +
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
    elements = reinterpret_cast<uint8_t*>(&next[1]);
    ptr_consumer = &consumers[consumer];

    pending_seq_consumer = 0;
    pending_seq_cursor = 0;

    pending_seq_next = 1;
    pending_seq_next_end = 0;
}

Disruptor::~Disruptor()
{
    Release();
}

int Disruptor::Release()
{
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
    if (Release() < 0)
    {
        ThrowErrnoError(info, "Failed to unmap shared memory"); //LCOV_EXCL_LINE
    }
}

template<typename Array, template<typename> typename Buffer>
Array Disruptor::ConsumeNewSync(const Napi::Env& env,
                                bool retry,
                                sequence_t &start)
{
    // Return all elements [&consumers[consumer], cursor)

    // Commit previous consume
    ConsumeCommit();

    do
    {
        sequence_t seq_consumer = __sync_val_compare_and_swap(ptr_consumer, 0, 0);
        sequence_t seq_cursor = __sync_val_compare_and_swap(cursor, 0, 0);
        sequence_t pos_consumer = seq_consumer % num_elements;
        sequence_t pos_cursor = seq_cursor % num_elements;

        if (pos_cursor > pos_consumer)
        {
            Array r = Array::New(env);

            r.Set(0U, Buffer<uint8_t>::New(
                env,
                elements + pos_consumer * element_size,
                (pos_cursor - pos_consumer) * element_size));

            UpdatePending(seq_consumer, seq_cursor);
            start = seq_consumer;
            return r;
        }

        if (seq_cursor != seq_consumer)
        {
            Array r = Array::New(env);

            r.Set(0U, Buffer<uint8_t>::New(
                env,
                elements + pos_consumer * element_size,
                (num_elements - pos_consumer) * element_size));

            if (pos_cursor > 0)
            {
                r.Set(1U, Buffer<uint8_t>::New(
                    env,
                    elements,
                    pos_cursor * element_size));
            }

            UpdatePending(seq_consumer, seq_cursor);
            start = seq_consumer;
            return r;
        }
    }
    while (retry);

    start = 0;
    return Array::New(env);
}

Napi::Value Disruptor::ConsumeNewSync(const Napi::CallbackInfo& info)
{
    sequence_t start;
    return ConsumeNewSync<Napi::Array, Napi::Buffer>(info.Env(), spin, start);
}

class ConsumeNewAsyncWorker :
    public DisruptorAsyncWorker<AsyncArray<AsyncBuffer<uint8_t>>,
                                sequence_t,
                                AsyncUndefined>
{
public:
    ConsumeNewAsyncWorker(Disruptor *disruptor,
                          const Napi::Function& callback) :
        DisruptorAsyncWorker<AsyncArray<AsyncBuffer<uint8_t>>,
                             sequence_t,
                             AsyncUndefined>(
            disruptor, callback)
    {
        arg1 = 0;
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ConsumeNewSync<AsyncArray<AsyncBuffer<uint8_t>>, AsyncBuffer>(Env(), false, arg1);
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
    Napi::Array r = ConsumeNewSync<Napi::Array, Napi::Buffer>(
        info.Env(), false, start);

    if (r.Length() > 0)
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
        r = __sync_bool_compare_and_swap(ptr_consumer,
                                         pending_seq_consumer,
                                         pending_seq_cursor);
        pending_seq_cursor = 0;
    }

    return r;
}

template<template<typename> typename Buffer>
Buffer<uint8_t> Disruptor::ProduceClaimSync(const Napi::Env& env,
                                            bool retry,
                                            sequence_t& out_next,
                                            sequence_t& out_next_end)
{
    do
    {
        sequence_t seq_next = __sync_val_compare_and_swap(next, 0, 0);

        bool can_claim = true;

        for (uint32_t i = 0; i < num_consumers; ++i)
        {
            sequence_t seq_consumer = __sync_val_compare_and_swap(&consumers[i], 0, 0);

            if ((seq_next - seq_consumer) >= num_elements)
            {
                can_claim = false;
                break;
            }
        }

        if (can_claim &&
            __sync_bool_compare_and_swap(next, seq_next, seq_next + 1))
        {
            Buffer<uint8_t> r = Buffer<uint8_t>::New(
                env,
                elements + (seq_next % num_elements) * element_size,
                element_size);

            UpdateSeqNext(seq_next, seq_next);
            out_next = seq_next;
            out_next_end = seq_next;

            return r;
        }
    }
    while (retry);

    UpdateSeqNext(1, 0);
    out_next = 1;
    out_next_end = 0;
    return Buffer<uint8_t>::New(env, 0);
}

Napi::Value Disruptor::ProduceClaimSync(const Napi::CallbackInfo& info)
{
    sequence_t seq_next, seq_next_end;
    return ProduceClaimSync<Napi::Buffer>(
        info.Env(), spin, seq_next, seq_next_end);
}

class ProduceClaimAsyncWorker :
    public DisruptorAsyncWorker<AsyncBuffer<uint8_t>, sequence_t, sequence_t>
{
public:
    ProduceClaimAsyncWorker(Disruptor *disruptor,
                            const Napi::Function& callback) :
        DisruptorAsyncWorker<AsyncBuffer<uint8_t>, sequence_t, sequence_t>(
            disruptor, callback)
    {
        arg1 = 1;
        arg2 = 0;
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ProduceClaimSync<AsyncBuffer>(
            Env(), false, arg1, arg2);
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
    Napi::Buffer<uint8_t> r = ProduceClaimSync<Napi::Buffer>(
        info.Env(), false, seq_next, seq_next_end);
    if (r.Length() > 0)
    {
        return r;
    }

    ProduceClaimAsync(info);
    return info.Env().Undefined();
}

template<typename Array, template<typename> typename Buffer>
void Disruptor::ProduceGetBuffers(const Napi::Env& env,
                                  sequence_t seq_next,
                                  sequence_t seq_next_end,
                                  Array& r)
{
    sequence_t pos_next = seq_next % num_elements;
    sequence_t pos_next_end = seq_next_end % num_elements;

    if (pos_next_end < pos_next)
    {
        r.Set(0U, Buffer<uint8_t>::New(
            env,
            elements + pos_next * element_size,
            (num_elements - pos_next) * element_size));

        r.Set(1U, Buffer<uint8_t>::New(
            env,
            elements,
            (pos_next_end + 1) * element_size));
    }
    else
    {
        r.Set(0U, Buffer<uint8_t>::New(
            env,
            elements + pos_next * element_size,
            (pos_next_end - pos_next + 1) * element_size));
    }

    UpdateSeqNext(seq_next, seq_next_end);
}

template<typename Array, template<typename> typename Buffer>
Array Disruptor::ProduceClaimManySync(const Napi::Env& env,
                                      uint32_t n,
                                      bool retry,
                                      sequence_t& out_next,
                                      sequence_t& out_next_end)
{
    do
    {
        sequence_t seq_next = __sync_val_compare_and_swap(next, 0, 0);
        sequence_t seq_next_end = seq_next + n - 1;

        bool can_claim = true;

        for (uint32_t i = 0; i < num_consumers; ++i)
        {
            sequence_t seq_consumer = __sync_val_compare_and_swap(&consumers[i], 0, 0);

            if ((seq_next_end - seq_consumer) >= num_elements)
            {
                can_claim = false;
                break;
            }
        }

        if (can_claim &&
            __sync_bool_compare_and_swap(next, seq_next, seq_next_end + 1))
        {
            Array r = Array::New(env);
            ProduceGetBuffers<Array, Buffer>(env, seq_next, seq_next_end, r);
            out_next = seq_next;
            out_next_end = seq_next_end;
            return r;
        }
    }
    while (retry);

    UpdateSeqNext(1, 0);
    out_next = 1;
    out_next_end = 0;
    return Array::New(env);
}

Napi::Value Disruptor::ProduceClaimManySync(const Napi::CallbackInfo& info)
{
    sequence_t seq_next, seq_next_end;
    return ProduceClaimManySync<Napi::Array, Napi::Buffer>(
        info.Env(), info[0].As<Napi::Number>(), spin, seq_next, seq_next_end);
}

class ProduceClaimManyAsyncWorker :
    public DisruptorAsyncWorker<AsyncArray<AsyncBuffer<uint8_t>>,
                                sequence_t,
                                sequence_t>
{
public:
    ProduceClaimManyAsyncWorker(Disruptor *disruptor,
                                const Napi::Function& callback,
                                uint32_t n) :
        DisruptorAsyncWorker<AsyncArray<AsyncBuffer<uint8_t>>,
                             sequence_t,
                             sequence_t>(
            disruptor, callback),
        n(n)
    {
        arg1 = 1;
        arg2 = 0;
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ProduceClaimManySync<AsyncArray<AsyncBuffer<uint8_t>>, AsyncBuffer>(
            Env(), n, false, arg1, arg2);
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
    Napi::Array r = ProduceClaimManySync<Napi::Array, Napi::Buffer>(
        info.Env(), n, false, seq_next, seq_next_end);
    if (r.Length() > 0)
    {
        return r;
    }

    ProduceClaimManyAsync(info, n);
    return info.Env().Undefined();
}

Napi::Value Disruptor::ProduceRecover(const Napi::CallbackInfo& info)
{
    sequence_t seq_next = info[0].As<Napi::Number>().Int64Value();
    sequence_t seq_next_end = info[1].As<Napi::Number>().Int64Value();

    Napi::Array r = Napi::Array::New(info.Env());

    if ((seq_next <= seq_next_end) &&
        (__sync_val_compare_and_swap(cursor, 0, 0) <= seq_next) &&
        (__sync_val_compare_and_swap(next, 0, 0) > seq_next_end))
    {
        ProduceGetBuffers<Napi::Array, Napi::Buffer>(
            info.Env(), seq_next, seq_next_end, r);
    }

    return r;
}

template<typename Boolean>
Boolean Disruptor::ProduceCommitSync(const Napi::Env& env,
                                     sequence_t seq_next,
                                     sequence_t seq_next_end,
                                     bool retry)
{
    if (seq_next <= seq_next_end)
    {
        do
        {
            if (__sync_bool_compare_and_swap(cursor, seq_next, seq_next_end + 1))
            {
                return Boolean::New(env, true);
            }
        }
        while (retry);
    }

    return Boolean::New(env, false);
}

void Disruptor::UpdateSeqNext(sequence_t seq_next, sequence_t seq_next_end)
{
    pending_seq_next = seq_next;
    pending_seq_next_end = seq_next_end;
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
    public DisruptorAsyncWorker<AsyncBoolean, AsyncUndefined, AsyncUndefined>
{
public:
    ProduceCommitAsyncWorker(Disruptor *disruptor,
                             const Napi::Function& callback,
                             sequence_t seq_next,
                             sequence_t seq_next_end) :
        DisruptorAsyncWorker<AsyncBoolean, AsyncUndefined, AsyncUndefined>(
            disruptor, callback),
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
    if (r)
    {
        return r;
    }

    ProduceCommitAsync(info, seq_next, seq_next_end, cb_arg);
    return info.Env().Undefined();
}

Napi::Value Disruptor::GetConsumers(const Napi::CallbackInfo& info)
{
    return Napi::Buffer<sequence_t>::New(info.Env(), consumers, num_consumers);
}

Napi::Value Disruptor::GetCursor(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __sync_val_compare_and_swap(cursor, 0, 0));
}

Napi::Value Disruptor::GetNext(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __sync_val_compare_and_swap(next, 0, 0));
}

Napi::Value Disruptor::GetElements(const Napi::CallbackInfo& info)
{
    return Napi::Buffer<uint8_t>::New(info.Env(), elements, num_elements * element_size);
}

Napi::Value Disruptor::GetConsumer(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __sync_val_compare_and_swap(ptr_consumer, 0, 0));
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

Napi::Value Disruptor::GetElementSize(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), element_size);
}

Napi::Object Disruptor::Initialize(Napi::Env env, Napi::Object exports)
{
    exports.Set("Disruptor", DefineClass(env, "Disruptor",
    {
        InstanceMethod("produceClaim", &Disruptor::ProduceClaim),
        InstanceMethod("produceClaimSync", &Disruptor::ProduceClaimSync),
        InstanceMethod("produceClaimMany", &Disruptor::ProduceClaimMany),
        InstanceMethod("produceClaimManySync", &Disruptor::ProduceClaimManySync),
        InstanceMethod("produceCommit", &Disruptor::ProduceCommit),
        InstanceMethod("produceCommitSync", &Disruptor::ProduceCommitSync),
        InstanceMethod("produceRecover", &Disruptor::ProduceRecover),
        InstanceMethod("consumeNew", &Disruptor::ConsumeNew),
        InstanceMethod("consumeNewSync", &Disruptor::ConsumeNewSync),
        InstanceMethod("consumeCommit", &Disruptor::ConsumeCommit),
        InstanceMethod("release", &Disruptor::Release),
        InstanceAccessor("prevConsumeStart", &Disruptor::GetPendingSeqConsumer, nullptr),
        InstanceAccessor("prevClaimStart", &Disruptor::GetPendingSeqNext, nullptr),
        InstanceAccessor("prevClaimEnd", &Disruptor::GetPendingSeqNextEnd, nullptr),
        InstanceAccessor("elementSize", &Disruptor::GetElementSize, nullptr),

        // For testing only
        InstanceAccessor("consumers", &Disruptor::GetConsumers, nullptr),
        InstanceAccessor("cursor", &Disruptor::GetCursor, nullptr),
        InstanceAccessor("next", &Disruptor::GetNext, nullptr),
        InstanceAccessor("elements", &Disruptor::GetElements, nullptr),
        InstanceAccessor("consumer", &Disruptor::GetConsumer, nullptr),
        InstanceAccessor("prevConsumeNext", &Disruptor::GetPendingSeqCursor, nullptr),
        InstanceMethod("consumeNewAsync", &Disruptor::ConsumeNewAsync),
        InstanceMethod("produceClaimAsync", &Disruptor::ProduceClaimAsync),
        InstanceMethod("produceClaimManyAsync", &Disruptor::ProduceClaimManyAsync),
        InstanceMethod("produceCommitAsync", &Disruptor::ProduceCommitAsync),
    }));

    return exports;
}

Napi::Object Initialize(Napi::Env env, Napi::Object exports)
{
    return Disruptor::Initialize(env, exports);
}

NODE_API_MODULE(disruptor, Initialize)
