# mvThreadPool
<sub>(This library is available under a free and permissive license)</sub>

mvThreadPool is a **simple to use header only C++ threadpool**.

### Usage
To use this library, just include _mvThreadPool.h_ in your project. The basic usage can be seen below:

```Cpp

int main()
{

  // create threadpool
  Marvel::mvThreadPool threadpool();
  
  // submit function
  threadpool->submit(&SomeFunction);
  
  // submit function with arguments
  threadpool->submit(std::bind(&SomeVarFunction, arg1, arg2, ...));
  
  // submit a method
  threadpool->submit(std::bind(&SomeClass::SomeMethod, classInstance, arg1, arg2, ...));
  
}

```

### Features

- Work Stealing
- Task Waiting

### Notes

- If the submitted function/method returns a value, the _submit_ method will return a future.
