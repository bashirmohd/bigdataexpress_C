# ----------------------------------------------------------------------

find_package(Sphinx REQUIRED)

# configured documentation tools and intermediate build results
set(BINARY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_build")

file(COPY
  "${CMAKE_CURRENT_SOURCE_DIR}/_static"
  DESTINATION "${BINARY_BUILD_DIR}")

# Sphinx cache with pickled ReST documents
set(SPHINX_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_doctrees")

# ----------------------------------------------------------------------

# HTML output directory
set(SPHINX_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/html")
set(SPHINX_SINGLE_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/singlehtml")

file(COPY
  "${CMAKE_CURRENT_SOURCE_DIR}/conf.py"
  DESTINATION "${BINARY_BUILD_DIR}")

add_custom_target(bde_html_portal_docs ALL
  ${SPHINX_EXECUTABLE}
  -q -b html
  -c "${BINARY_BUILD_DIR}"
  -d "${SPHINX_CACHE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${SPHINX_HTML_DIR}"
  COMMENT "Building multi-page HTML documentation with Sphinx")

add_custom_target(bde_single_html_portal_docs ALL
  ${SPHINX_EXECUTABLE}
  -q -b singlehtml
  -c "${BINARY_BUILD_DIR}"
  -d "${SPHINX_CACHE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${SPHINX_SINGLE_HTML_DIR}"
  COMMENT "Building single-page HTML documentation with Sphinx")

# ----------------------------------------------------------------------

if (BDE_ENABLE_PDF_MANUAL)

  # Set latex output directory
  set(SPHINX_LATEX_DIR "${CMAKE_CURRENT_BINARY_DIR}/latex")

  add_custom_target(bde_portal_latex_docs ALL
    ${SPHINX_EXECUTABLE}
    -q -b latex
    -c "${BINARY_BUILD_DIR}"
    -d "${SPHINX_CACHE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SPHINX_LATEX_DIR}"
    COMMENT "Building LaTeX documentation with Sphinx")

  # TODO: find latexmk program
  add_custom_target(bde_portal_pdf_doc ALL
    latexmk
    -cd # with `-cd` latexmk will change to ${SPHINX_LATEX_DIR}
    -pdf -dvi- -ps-  "${SPHINX_LATEX_DIR}/BigDataExpressPortal.tex"
    DEPENDS "${SPHINX_LATEX_DIR}/BigDataExpressPortal.tex"
    COMMENT "Building PDF documentation with latexmk")

endif ()

# ----------------------------------------------------------------------
