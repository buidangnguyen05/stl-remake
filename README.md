# stl-remake

A re-implementation of standard library containers in C++23, written to learn how they work inside and where they
could be faster. Matching `std` rule for rule is not a goal. `std` is the reference for correct results in tests.

## What to write, and what to call from `std`

**Rule of thumb:** write it yourself if it touches raw memory or the lifetime of objects. Use `std` for everything
else, unless the task is about that very thing.

Raw memory and object lifetimes are where container internals live: allocating, constructing and destroying in place,
rolling back when a constructor throws, and growing. Everything else is plumbing.

| Category                      | Examples                                                                                                                                                 | Rule                                                                                                                       |
|-------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------|
| Language support              | `std::construct_at`, `std::destroy_at`, `std::move`, `std::forward`, `std::launder`, `<type_traits>`, `<concepts>`, `std::initializer_list`, `std::numeric_limits` | **Use.** Some of these need compiler support, and none of them is what this project is about.                              |
| Small utilities               | `std::min`, `std::max`, `std::swap`, `std::exchange`, exception types such as `std::length_error`                                                         | **Use.**                                                                                                                   |
| Algorithms on raw memory      | `std::uninitialized_copy`, `std::uninitialized_fill_n`, `std::uninitialized_value_construct_n`, `std::uninitialized_move`, `std::destroy`, `std::ranges::destroy` | **Write your own**, unless there is a very good reason not to. These construct and destroy directly, so they would bypass the container's own helpers. |
| Algorithms on live objects    | `std::copy`, `std::move_backward`, `std::equal`, `std::lexicographical_compare`, `std::rotate`                                                           | **Use for now.** Once the containers work, come back and write your own versions.                                          |
| Containers and smart pointers | `std::vector`, `std::list`, `std::unique_ptr`                                                                                                            | **Do not use** inside the library. In tests, use them freely as the reference to compare against.                          |

Inside a container, only a few private helpers allocate, free, construct or destroy. Everything else calls those
helpers. Then adding allocator support changes the helpers, not the whole class.
