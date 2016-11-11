- D0315R2
    + Make removed text red
    + Green an underlined for additions
    + Give lambdas linkage in some purposes
    + section 6. keep the note around the intent part only
    + In [temp.over.link], reword to something like this:
        To make sure this does not happen, we propose amending \textbf{[temp.over.link]
        14.5.6.1/5} as follows:
        \begin{quote}
          Two expressions involving template parameters are considered equivalent
          if two function definitions containing the expressions would satisfy the
          one-definition rule (3.2), except that the tokens used to name the template
          parameters may differ as long as a token used to name a template parameter
          in one expression is replaced by another token that names the same template
          parameter in the other expression. \added{Two lambda-expressions appearing
          in full expressions involving template parameters are never considered
          equivalent. [ \textit{Note:} The intent is to avoid lambda-expressions
          appearing in the signature of a function template with external linkage.
          \textit{-- end note} ]}
        \end{quote}


Scratchpad for string literals:

        template <int is[]>
        struct foo { };

        template <char cs[]>
        struct bar { };

    foo<"abcd"> x1;
    foo<{'a', 'b', 'c'}> x2;
