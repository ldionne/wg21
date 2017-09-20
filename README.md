## Louis Dionne's proposals for the C++ Standards Committee

The top level of this repository contains the source for the proposals, and
the `generated/` directory contains the generated proposals (html or pdf).
The source for proposals that are not being worked on anymore are in the
`done/` subdirectory so I can keep my sanity between meetings.

To generate the PDF for a proposal with source in `pXXXX.tex`, type
`make pXXXX`. This will generate the appropriate PDF file `pXXXXrN.pdf`
or `dXXXXrN.pdf` in the `generated/` folder. This requires a LaTeX
distribution, such as [MacTeX][].


[MacTeX]: https://www.tug.org/mactex
