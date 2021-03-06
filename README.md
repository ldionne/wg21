## Louis Dionne's proposals for the C++ Standards Committee

The top level of this repository contains the source for the proposals, and
the `generated/` directory contains the generated proposals (html or pdf).
The source for proposals that are not being worked on anymore are in the
`done/` subdirectory so I can keep my sanity between meetings.

To generate the PDF or HTML for a proposal with source in `pXXXX.{tex,bs}`,
type `make pXXXX.{tex,bs}`. This will generate the appropriate PDF or HTML
file `pXXXXrN.<ext>` or `dXXXXrN.<ext>` in the `generated/` folder. This
requires a LaTeX distribution, such as [MacTeX][] or an internet connection,
depending on whether the source uses LaTeX or [Bikeshed][].

#### TODO list
For Kona:
- Paper to make `std::unordered_map` `constexpr`
- Paper for making `std::variant` and `std::optional` `constexpr`
- Paper to claim header names without extension and with double underscores
- Paper to prohibit user specializations of `std::pointer_traits<T*>`

Eventually:
- Paper to add parameter pack support for anything that has begin() and end(), and is constexpr? Or maybe instead a variation of `for ...`?
- Paper for unified product type access

[Bikeshed]: https://tabatkins.github.io/bikeshed
[MacTeX]: https://www.tug.org/mactex
