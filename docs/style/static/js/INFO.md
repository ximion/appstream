### AppStream Documentation JS Directory

You can place a minified copy of [Highlight.js](https://highlightjs.org/) in this
directory to enable source-code highlighting in the documentation.

In case a system-wide copy of `highlight.min.js` and its CSS files exists in
`/usr/share/javascript/highlight.js/`, the buildsystem will automatically detect that
and create symbolic links to these files when installing the documentation.
