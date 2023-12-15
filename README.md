# STPerf

A tiny C/C++ performance profiling tool. Native in C++11. It's C-API allows for its implementation in almost any environment.

### Installing
##### Directly
To use `stperf` you can simply copy `stperf.cpp` and `stperf.h` to your project if you have a C++11 compliant compiler. Done!

##### Using CMake
```CMake
include(FetchContent)
FetchContent_Declare(
    stperf
    GIT_REPOSITORY https://github.com/lPrimemaster/stperf.git
)
FetchContent_MakeAvailable(stperf)

add_executable(<your-exec> <your-sources>)
target_include_directories(<your-exec> PRIVATE ${stperf_SOURCE_DIR})
target_link_libraries(<your-exec> PRIVATE stperf)
```
Now you can just `#include <stperf.h>`.

All Done!
### How to use
- [From C++](From-C++)
- [From C/CAPI](From-C/CAPI)
#### From C++
Simply use the already defined preprocessor macros (in `stperf.h`) as the first statement on the function(s) you wish to profile.

That's it! You're done!
###### Example
```cpp
// Can also be a lambda function
int func_to_profile(int arg)
{
    ST_PROF;
    // ... function code ...
    return arg;
}

int func_partial_profile(int arg)
{
    ST_PROF;
    {
        ST_PROF_NAMED("partial_scope_1"); // this profiles until scope end
        // ...
    }

    for(int i = 0; i < 10; i++)
    {
        ST_PROF_NAMED("partial_scope_for"); // again until the end of scope
        // ...
    }
    // if using st_prof multiple times inside the same functin it will have the same name
    // using ST_PROF_NAMED is advised for these cases (you can use __func__ for more specific names eg.)
    return arg;
}

int main(void)
{
    // We need to make a scope to be able to see the statistics here
    // Alternatively, you can call start/stop manually
    // Refer to the docs
    {
        ST_PROF;
        (void)func_to_profile(0);
        (void)func_partial_profile(0);
    }

    // Print performance statistics
    std::cout << cag::PerfTimer::GetCallTreeString(cag::PerfTimer::GetCallTree()) << std::endl;
    return 0;
}
```
###### Example Output (From above)

```
-> [main() | x1] Execution time : 8.156us (100%).
        -> [func_partial_profile() | x1] Execution time : 4.328us (53.07%).
                -> [partial_scope_for | x10] Execution time : 1.373us (16.83%).
                -> [partial_scope_1 | x1] Execution time : 100ns (1.226%).
        -> [func_to_profile() | x1] Execution time : 390ns (4.782%).
```

#### From C/CAPI
Because there is no custom object construction/destruction behaviour in C you have to place start/stop instructions in your scope.
This however allows for a finer control. C++ also has its own functions to do so if you wish.
###### Example
```c
// Can also be a lambda function
int func_to_profile(int arg)
{
    uint64_t handle = stperf_StartProf(__func__, __LINE__, NULL);
    // ... function code ...
    stperf_StopProf(handle);
    return arg;
}

int func_partial_profile(int arg)
{
    uint64_t handle = stperf_StartProf(__func__, __LINE__, NULL);

    uint64_t handle_partial = stperf_StartProf("partial_scope_1", __LINE__, NULL);
    // ...
    stperf_StopProf(handle_partial);

    for(int i = 0; i < 10; i++)
    {
        uint64_t handle_for = stperf_StartProf("partial_scope_for", __LINE__, NULL); 
        // ...
        stperf_StopProf(handle_for);
    }
    
    stperf_StopProf(handle);
    return arg;
}

int main(void)
{
    uint64_t handle = stperf_StartProf(__func__, __LINE__, NULL);
    (void)func_to_profile(0);
    (void)func_partial_profile(0);
    stperf_StopProf(handle_for);

    // Print performance statistics
    stperf_PerfNodeList nodes = stperf_GetCallTree();
    const char* report = stperf_GetCallTreeString(nodes);

    puts(report); 

    stperf_FreeCallTreeString(report);
    stperf_FreeCallTree(nodes);
    return 0;
}
```
###### Example Output (From above)

```
-> [main() | x1] Execution time : 8.156us (100%).
        -> [func_partial_profile() | x1] Execution time : 4.328us (53.07%).
                -> [partial_scope_for | x10] Execution time : 1.373us (16.83%).
                -> [partial_scope_1 | x1] Execution time : 100ns (1.226%).
        -> [func_to_profile() | x1] Execution time : 390ns (4.782%).
```

