 _     _             _       _
| |__ (_) __ _    __| | __ _| |_ __ _    _____  ___ __  _ __ ___  ___ ___
| '_ \| |/ _` |  / _` |/ _` | __/ _` |  / _ \ \/ / '_ \| '__/ _ \/ __/ __|
| |_) | | (_| | | (_| | (_| | || (_| | |  __/>  <| |_) | | |  __/\__ \__ \
|_.__/|_|\__, |  \__,_|\__,_|\__\__,_|  \___/_/\_\ .__/|_|  \___||___/___/
         |___/                                   |_|

         https://bigdataexpress.fnal.gov/

--------------------------------------------------------------------------

This source tree contains sources for Fermilab Big Data Express server
and agent components.  Please see the website to learn more about the
project.

To build this source tree, you will need:

  - A relatively new GNU/Linux distribution.  We have used Scientific
    Linux 7.3, CentOS 7, Ubuntu 16.04 and 18.04, among others.

  - A C++ compiler that supports C++11.  We use GCC.

  - CMake version 3.11 or better.

  - Libraries: zlib, mosquitto, and OpenSSL (1.0.2x)

There are certain other dependencies too that are downloaded and built
locally.  To do that, run the shell script in the top-level directory:

  ./bootstrap_libraries.sh

To build the source tree, do this:

   mkdir build
   cd build
   cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
   make

If would like to enable building the test suite, you should pass the
"-DBDE_ENABLE_TESTING=ON" parameter to cmake.

You can also use Ninja <https://ninja-build.org/> for faster buids.
Just use the appropriate generator with cmake:

  cmake -G Ninja ..
  ninja

The generated binaries are statically linked against the locally
installed libraries, and should run on GNU/Linux distributions where
other ABIs (glibc, zlib, OpenSSL, etc.) are compatible.


Generating Documentation
--------------------------------------------------------------------------

BDE installation manual resides in "docs" directory.  It uses Sphinx
document processor to generate HTML documents (and PDF output too)
from documents written in reStructuredText (reST) format.

For details about Sphinx and reST, please visit these sites:

 - https://www.sphinx-doc.org/en/master/index.html
 - http://docutils.sourceforge.net/rst.html
 - https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html

By default, the generated HTML uses "Read the Documents" theme:

 - https://github.com/readthedocs/sphinx_rtd_theme

See Sphinx website for methods to install Sphinx.  Assuming you have
Python 3.5+ and pip3, you should be able to do this:

  pip3 install -U Sphinx
  pip3 install -U sphinx_rtd_theme

For generating HTML output, we have two methods: as part of CMake
build system, or outside of it.

Generating HTML output outside of the CMake build system may be more
convenient if you are working on, say, a macOS machine where you can't
run the rest of the build process.  In that case you can just do:

  cd docs/design
  make html

And the output will be in "docs/design/_build/html".

Sphinx can generate documents in several other formats too:
single-page HTML, LaTeX, PDF, and a bunch of others.  You will need to
have the appropriate software installed.

We can also generate documentation as part of the CMake build process:

  cd build
  cmake -DBDE_ENABLE_MANUAL=ON -DBDE_ENABLE_PDF_MANUAL=ON ..
  cd docs
  make

We can also run "make bde_html_docs" or "bde_single_html_docs" or
"bde_pdf_doc", if we need just one output format.

Some diagrams were created using https://draw.io, and then exported to
PNG format to be included in the documentation.

If you need to edit the diagrams, open the corresponding file with
.drawio extension in draw.io website, make your changes, download the
resulting PNG and .drawio files, and replace the diagrams you want to
replace.
