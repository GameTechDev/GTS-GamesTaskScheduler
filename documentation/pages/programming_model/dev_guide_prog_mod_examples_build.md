Building the Examples {#dev_guide_prog_mod_examples_build}
============================

# Download the Source Code

Clone [the repository](https://github.com/GameTechDev/GTS-GamesTaskScheduler.git)

~~~sh
git clone https://github.com/GameTechDev/GTS-GamesTaskScheduler.git
~~~

# Build the Examples

## Windows

- Generate a Microsoft Visual Studio solutions:
~~~bat
premake/gts_examples.bat
~~~
 - This will create the directory "_build", which contains several solutions.

- Open a solution:
~~~bat
_build/gts_examples/<vs*>/msvc/gts_examples.sln
~~~

- Select your target platform and build the solution. This will build GTS and the example projects.
- The build products will be located at:
~~~bat
_build/gts_examples/<vs*>/msvc/<build_flavor>
~~~