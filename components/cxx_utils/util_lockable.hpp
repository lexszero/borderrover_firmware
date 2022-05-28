#pragma once

#include <thread>
#include <shared_mutex>
#include <iostream>
#include <type_traits>

/**
 * `Lockable` implements a convenience wrapper for shared mutex to protect class
 * state.
 *
 * Derive your classes from `Lockable` and use `auto lock = take_unique_lock()` to
 * take the lock in methods.
 *
 * Set `debug` to `true` to get console messages when locks are being
 * taken/released
 */
template <class Mutex = std::shared_mutex, bool debug = false>
class Lockable {
    private:
        template <typename LockType>
        class lock_debug_wrapper : LockType
        {
        public:
            explicit lock_debug_wrapper(Lockable& l) :
                LockType(l.mutex),
                lockable(l)
            {
                std::cerr << lockable <<
                    " [" << std::this_thread::get_id() << "] take " <<
                    lock_type_str() << std::endl;
            }

            ~lock_debug_wrapper()
            {
                std::cerr << lockable <<
                    " [" << std::this_thread::get_id() << "] release " <<
                    lock_type_str() << std::endl;
            }
        private:
            const Lockable& lockable;
            constexpr const char *lock_type_str() {
                return std::is_same<LockType, std::shared_lock<Mutex>>::value
                    ? "shared"
                    : (std::is_same<LockType, std::unique_lock<Mutex>>::value
                            ? "unique"
                            : "unknown");
            }
        };

        using base_shared_lock = std::shared_lock<Mutex>;
        using base_unique_lock = std::unique_lock<Mutex>;

        template <typename base_lock>
        using if_debug_wrap = typename std::conditional<debug,
              lock_debug_wrapper<base_lock>,
              base_lock>::type;


        template <typename base_lock>
        struct lock_factory_default {
            using type = base_lock;
            static auto make(Lockable& l) {
                return type(l.mutex);
            }
        };

        template <typename base_lock>
        struct lock_factory_debug {
            using type = lock_debug_wrapper<base_lock>;
            static auto make(Lockable& l) {
                return type(l);
            }
        };

        template <typename base_lock>
        using lock_factory = typename std::conditional<debug,
              lock_factory_debug<base_lock>,
              lock_factory_default<base_lock>>::type;

    public:
        Lockable() = default;
        explicit Lockable(const char *name) :
            name(name)
        {}

        using shared_lock = typename lock_factory<base_shared_lock>::type;
        using unique_lock = typename lock_factory<base_unique_lock>::type;

        auto take_shared_lock() const {
            return lock_factory<base_shared_lock>::make(const_cast<Lockable&>(*this));
        }

        auto take_unique_lock() {
            return lock_factory<base_unique_lock>::make(*this);
        }

        const char *name;

    private:
        mutable Mutex mutex;


//    friend std::ostream& operator<<(std::ostream& stream, const Lockable<Mutex>& l);
};

template <typename Mutex>
std::ostream& operator<<(std::ostream& stream, const Lockable<Mutex>& l)
{
    return stream << "<Lockable " << l.name << ">";
}
