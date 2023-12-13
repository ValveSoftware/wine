/*
 * Copyright 2016 Jacek Caban for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

var compat_version;
var tests = [];

ok(performance.timing.navigationStart > 0, "navigationStart <= 0");
ok(performance.timing.fetchStart == performance.timing.navigationStart, "fetchStart != navigationStart");
ok(performance.timing.domainLookupStart >= performance.timing.fetchStart, "domainLookupStart < fetchStart");
ok(performance.timing.domainLookupEnd >= performance.timing.domainLookupStart, "domainLookupEnd < domainLookupStart");
ok(performance.timing.connectStart >= performance.timing.domainLookupEnd, "connectStart < domainLookupEnd");
ok(performance.timing.connectEnd >= performance.timing.connectStart, "connectEnd < connectStart");
ok(performance.timing.requestStart >= performance.timing.connectEnd, "requestStart < connectEnd");
ok(performance.timing.responseStart >= performance.timing.requestStart, "responseStart < requestStart");
ok(performance.timing.responseEnd >= performance.timing.responseStart, "responseEnd < responseStart");
ok(performance.timing.domLoading >= performance.timing.responseEnd, "domLoading < responseEnd");
ok(performance.timing.domInteractive === 0, "domInteractive != 0");
ok(performance.timing.domComplete === 0, "domComplete != 0");
ok(performance.timing.domContentLoadedEventStart === 0, "domContentLoadedEventStart != 0");
ok(performance.timing.domContentLoadedEventEnd === 0, "domContentLoadedEventEnd != 0");
ok(performance.timing.loadEventStart === 0, "loadEventStart != 0");
ok(performance.timing.loadEventEnd === 0, "loadEventEnd != 0");
ok(performance.timing.unloadEventStart === 0, "unloadEventStart != 0");
ok(performance.timing.unloadEventEnd === 0, "unloadEventEnd != 0");
ok(performance.timing.redirectStart === 0, "redirectStart != 0");
ok(performance.timing.redirectEnd === 0, "redirectEnd != 0");
ok(performance.timing.msFirstPaint === 0, "msFirstPaint != 0");

var pageshow_fired = false, pagehide_fired = false;
document.doc_unload_events_called = false;
window.onbeforeunload = function() { ok(false, "beforeunload fired"); };
window.onunload = function() {
    document.doc_unload_events_called = true;
    ok(document.readyState === "complete", "unload readyState = " + document.readyState);
    if(document.documentMode < 11)
        ok(pagehide_fired === false, "pagehide fired before unload");
    else
        ok(pagehide_fired === true, "pagehide not fired before unload");
};

if(window.addEventListener) {
    window.addEventListener("pageshow", function(e) {
        pageshow_fired = true;

        var r = Object.prototype.toString.call(e);
        ok(r === "[object PageTransitionEvent]", "pageshow toString = " + r);
        ok("persisted" in e, "'persisted' not in pageshow event");
        ok(document.readyState === "complete", "pageshow readyState = " + document.readyState);
        ok(performance.timing.loadEventEnd > 0, "loadEventEnd <= 0 in pageshow handler");
    }, true);

    window.addEventListener("pagehide", function(e) {
        pagehide_fired = true;
        ok(document.documentMode >= 11, "pagehide fired");

        var r = Object.prototype.toString.call(e);
        ok(r === "[object PageTransitionEvent]", "pagehide toString = " + r);
        ok("persisted" in e, "'persisted' not in pagehide event");
    }, true);

    document.addEventListener("visibilitychange", function() { ok(false, "visibilitychange fired"); });
    document.addEventListener("beforeunload", function() { ok(false, "beforeunload fired on document"); });
    document.addEventListener("unload", function() { ok(false, "unload fired on document"); });
}else {
    document.attachEvent("onbeforeunload", function() { ok(false, "beforeunload fired on document"); });
    document.attachEvent("onunload", function() { ok(false, "unload fired on document"); });
}

sync_test("performance timing", function() {
    ok(performance.timing.domInteractive >= performance.timing.domLoading, "domInteractive < domLoading");
    ok(performance.timing.domContentLoadedEventStart >= performance.timing.domInteractive, "domContentLoadedEventStart < domInteractive");
    ok(performance.timing.domContentLoadedEventEnd >= performance.timing.domContentLoadedEventStart, "domContentLoadedEventEnd < domContentLoadedEventStart");
    ok(performance.timing.domComplete >= performance.timing.domContentLoadedEventEnd, "domComplete < domContentLoadedEventEnd");
    ok(performance.timing.loadEventStart >= performance.timing.domComplete, "loadEventStart < domComplete");
    ok(performance.timing.loadEventEnd >= performance.timing.loadEventStart, "loadEventEnd < loadEventStart");
    ok(performance.navigation.type === 0, "navigation type = " + performance.navigation.type);
    ok(performance.navigation.redirectCount === 0, "redirectCount = " + performance.navigation.redirectCount);
});

sync_test("page transition events", function() {
    if(document.documentMode < 11)
        ok(pageshow_fired === false, "pageshow fired");
    else
        ok(pageshow_fired === true, "pageshow not fired");
    ok(pagehide_fired === false, "pagehide fired");

    if(document.body.addEventListener)
        document.body.addEventListener("unload", function() { ok(false, "unload fired on document.body"); });
    else
        document.body.attachEvent("onunload", function() { ok(false, "unload fired on document.body"); });
});

sync_test("builtin_toString", function() {
    var tags = [
        [ "abbr",            "Phrase" ],
        [ "acronym",         "Phrase" ],
        [ "address",         "Block" ],
     // [ "applet",          "Applet" ],  // makes Windows pop up a dialog box
        [ "article",         "" ],
        [ "aside",           "" ],
        [ "audio",           "Audio" ],
        [ "b",               "Phrase" ],
        [ "base",            "Base" ],
        [ "basefont",        "BaseFont" ],
        [ "bdi",             "Unknown" ],
        [ "bdo",             "Phrase" ],
        [ "big",             "Phrase" ],
        [ "blockquote",      "Block" ],
        [ "body",            "Body" ],
        [ "br",              "BR" ],
        [ "button",          "Button" ],
        [ "canvas",          "Canvas" ],
        [ "caption",         "TableCaption" ],
        [ "center",          "Block" ],
        [ "cite",            "Phrase" ],
        [ "code",            "Phrase" ],
        [ "col",             "TableCol" ],
        [ "colgroup",        "TableCol" ],
        [ "data",            "Unknown" ],
        [ "datalist",        "DataList", 10 ],
        [ "dd",              "DD" ],
        [ "del",             "Mod" ],
        [ "details",         "Unknown" ],
        [ "dfn",             "Phrase" ],
        [ "dialog",          "Unknown" ],
        [ "dir",             "Directory" ],
        [ "div",             "Div" ],
        [ "dl",              "DList" ],
        [ "dt",              "DT" ],
        [ "em",              "Phrase" ],
        [ "embed",           "Embed" ],
        [ "fieldset",        "FieldSet" ],
        [ "figcaption",      "" ],
        [ "figure",          "" ],
        [ "font",            "Font" ],
        [ "footer",          "" ],
        [ "form",            "Form" ],
        [ "frame",           "Frame" ],
        [ "frameset",        "FrameSet" ],
        [ "h1",              "Heading" ],
        [ "h2",              "Heading" ],
        [ "h3",              "Heading" ],
        [ "h4",              "Heading" ],
        [ "h5",              "Heading" ],
        [ "h6",              "Heading" ],
        [ "h7",              "Unknown" ],
        [ "head",            "Head" ],
        [ "header",          "" ],
        [ "hr",              "HR" ],
        [ "html",            "Html" ],
        [ "i",               "Phrase" ],
        [ "iframe",          "IFrame" ],
        [ "img",             "Image" ],
        [ "input",           "Input" ],
        [ "ins",             "Mod" ],
        [ "kbd",             "Phrase" ],
        [ "label",           "Label" ],
        [ "legend",          "Legend" ],
        [ "li",              "LI" ],
        [ "link",            "Link" ],
        [ "main",            "Unknown" ],
        [ "map",             "Map" ],
        [ "mark",            "" ],
        [ "meta",            "Meta" ],
        [ "meter",           "Unknown" ],
        [ "nav",             "" ],
        [ "noframes",        "" ],
        [ "noscript",        "" ],
        [ "object",          "Object" ],
        [ "ol",              "OList" ],
        [ "optgroup",        "OptGroup" ],
        [ "option",          "Option" ],
        [ "output",          "Unknown" ],
        [ "p",               "Paragraph" ],
        [ "param",           "Param" ],
        [ "picture",         "Unknown" ],
        [ "pre",             "Pre" ],
        [ "progress",        "Progress", 10 ],
        [ "q",               "Quote" ],
        [ "rp",              "Phrase" ],
        [ "rt",              "Phrase" ],
        [ "ruby",            "Phrase" ],
        [ "s",               "Phrase" ],
        [ "samp",            "Phrase" ],
        [ "script",          "Script" ],
        [ "section",         "" ],
        [ "select",          "Select" ],
        [ "small",           "Phrase" ],
        [ "source",          "Source" ],
        [ "span",            "Span" ],
        [ "strike",          "Phrase" ],
        [ "strong",          "Phrase" ],
        [ "style",           "Style" ],
        [ "sub",             "Phrase" ],
        [ "summary",         "Unknown" ],
        [ "sup",             "Phrase" ],
        [ "svg",             "Unknown" ],
        [ "table",           "Table" ],
        [ "tbody",           "TableSection" ],
        [ "td",              "TableDataCell" ],
        [ "template",        "Unknown" ],
        [ "textarea",        "TextArea" ],
        [ "tfoot",           "TableSection" ],
        [ "th",              "TableHeaderCell" ],
        [ "thead",           "TableSection" ],
        [ "time",            "Unknown" ],
        [ "title",           "Title" ],
        [ "tr",              "TableRow" ],
        [ "track",           "Track", 10 ],
        [ "tt",              "Phrase" ],
        [ "u",               "Phrase" ],
        [ "ul",              "UList" ],
        [ "var",             "Phrase" ],
        [ "video",           "Video" ],
        [ "wbr",             "" ],
        [ "winetest",        "Unknown" ]
    ];
    var v = document.documentMode, e;

    function test(msg, obj, name, tostr, ctor_name) {
        var s;
        if(obj.toString) {
            s = obj.toString();
            todo_wine_if(name !== "HTMLElement" && s === "[object HTMLElement]").
            ok(s === (tostr ? tostr : (v < 9 ? "[object]" : "[object " + name + "]")), msg + " toString returned " + s);
        }
        s = Object.prototype.toString.call(obj);
        todo_wine_if(name !== "HTMLElement" && s === "[object HTMLElement]").
        ok(s === (v < 9 ? "[object Object]" : "[object " + name + "]"), msg + " Object.toString returned " + s);

        if(v >= 9) {
            eval("var c = window." + name + ";");
            todo_wine_if(name !== "HTMLElement" && s === "[object HTMLElement]").
            ok(c !== undefined, name + " is undefined");
            if(!ctor_name) ctor_name = name;
            if(c === undefined) return;  /* todo_wine */

            s = Object.getPrototypeOf(obj);
            if(name === "Object") {
                ok(s === null, msg + "'s proto is not null: " + s);

                s = Object.prototype.toString.call(c);
                ok(s === "[object Function]", msg + " Object.toString on constructor returned " + s);
            }else {
                ok(s === c.prototype, msg + "'s proto is not its constructor's prototype");

                s = Object.prototype.toString.call(c);
                todo_wine_if(name !== "HTMLElement" && s === "[object HTMLElement]").
                ok(s === "[object " + ctor_name + "]", msg + " Object.toString on constructor returned " + s);

                s = Object.prototype.toString.call(c.prototype);
                todo_wine_if(name !== "HTMLElement" && s === "[object HTMLElementPrototype]").
                ok(s === "[object " + name + "Prototype]", msg + " Object.toString on constructor.prototype returned " + s);
            }
        }
    }

    for(var i = 0; i < tags.length; i++)
        if(tags[i].length < 3 || v >= tags[i][2])
            test("tag '" + tags[i][0] + "'", document.createElement(tags[i][0]), "HTML" + tags[i][1] + "Element");

    e = document.createElement("a");
    ok(e.toString() === "", "tag 'a' (without href) toString returned " + e.toString());
    e.href = "https://www.winehq.org/";
    test("tag 'a'", e, "HTMLAnchorElement", "https://www.winehq.org/");

    e = document.createElement("area");
    ok(e.toString() === "", "tag 'area' (without href) toString returned " + e.toString());
    e.href = "https://www.winehq.org/";
    test("tag 'area'", e, "HTMLAreaElement", "https://www.winehq.org/");

    e = document.createElement("style");
    document.body.appendChild(e);
    var sheet = v >= 9 ? e.sheet : e.styleSheet;
    if(v >= 9)
        sheet.insertRule("div { border: none }", 0);
    else
        sheet.addRule("div", "border: none", 0);

    e = document.createElement("p");
    e.className = "testclass    another ";
    e.textContent = "Test content";
    e.style.border = "1px solid black";
    document.body.appendChild(e);

    var txtRange = document.body.createTextRange();
    txtRange.moveToElementText(e);

    var clientRects = e.getClientRects();
    if(!clientRects) win_skip("getClientRects() is buggy and not available, skipping");

    var currentStyle = e.currentStyle;
    if(!currentStyle) win_skip("currentStyle is buggy and not available, skipping");

    // w10pro64 testbot VM throws WININET_E_INTERNAL_ERROR for some reason
    var localStorage;
    try {
        localStorage = window.localStorage;
    }catch(e) {
        ok(e.number === 0x72ee4 - 0x80000000, "localStorage threw " + e.number + ": " + e);
    }
    if(!localStorage) win_skip("localStorage is buggy and not available, skipping");

    test("attribute", document.createAttribute("class"), "Attr");
    if(false /* todo_wine */) test("attributes", e.attributes, "NamedNodeMap");
    test("childNodes", document.body.childNodes, "NodeList");
    if(clientRects) test("clientRect", clientRects[0], "ClientRect");
    if(clientRects) test("clientRects", clientRects, "ClientRectList");
    if(currentStyle) test("currentStyle", currentStyle, "MSCurrentStyleCSSProperties");
    test("document", document, v < 11 ? "Document" : "HTMLDocument");
    test("elements", document.getElementsByTagName("body"), "HTMLCollection");
    test("history", window.history, "History");
    test("implementation", document.implementation, "DOMImplementation");
    if(localStorage) test("localStorage", localStorage, "Storage");
    test("location", window.location, "Object", window.location.href);
    if(v >= 11 /* todo_wine */) test("mimeTypes", window.navigator.mimeTypes, v < 11 ? "MSMimeTypesCollection" : "MimeTypeArray");
    test("navigator", window.navigator, "Navigator");
    test("performance", window.performance, "Performance");
    test("performanceNavigation", window.performance.navigation, "PerformanceNavigation");
    test("performanceTiming", window.performance.timing, "PerformanceTiming");
    if(v >= 11 /* todo_wine */) test("plugins", window.navigator.plugins, v < 11 ? "MSPluginsCollection" : "PluginArray");
    test("screen", window.screen, "Screen");
    test("sessionStorage", window.sessionStorage, "Storage");
    test("style", document.body.style, "MSStyleCSSProperties");
    test("styleSheet", sheet, "CSSStyleSheet");
    test("styleSheetRule", sheet.rules[0], "CSSStyleRule");
    test("styleSheetRules", sheet.rules, "MSCSSRuleList");
    test("styleSheets", document.styleSheets, "StyleSheetList");
    test("textNode", document.createTextNode("testNode"), "Text", v < 9 ? "testNode" : null);
    test("textRange", txtRange, "TextRange");
    test("window", window, "Window", "[object Window]");
    test("xmlHttpRequest", new XMLHttpRequest(), "XMLHttpRequest", null, "Function");
    if(v < 10) {
        test("namespaces", document.namespaces, "MSNamespaceInfoCollection");
    }
    if(v < 11) {
        test("eventObject", document.createEventObject(), "MSEventObj");
        test("selection", document.selection, "MSSelection");
    }
    if(v >= 9) {
        test("computedStyle", window.getComputedStyle(e), "CSSStyleDeclaration");
        test("doctype", document.doctype, "DocumentType");

        test("Event", document.createEvent("Event"), "Event");
        test("CustomEvent", document.createEvent("CustomEvent"), "CustomEvent");
        test("KeyboardEvent", document.createEvent("KeyboardEvent"), "KeyboardEvent");
        test("MouseEvent", document.createEvent("MouseEvent"), "MouseEvent");
        test("UIEvent", document.createEvent("UIEvent"), "UIEvent");
    }
    if(v >= 10) {
        test("classList", e.classList, "DOMTokenList", "testclass    another ");
        test("console", window.console, "Console");
        test("mediaQueryList", window.matchMedia("(hover:hover)"), "MediaQueryList");
    }
    if(v >= 11) {
        test("crypto", window.msCrypto, "Crypto");
        test("MutationObserver", new window.MutationObserver(function() {}), "MutationObserver", null, "Function");
    }
    if(v >= 9) {
        document.body.innerHTML = "<!--...-->";
        test("comment", document.body.firstChild, "Comment");
    }
});

sync_test("builtin_obj", function() {
    var v = document.documentMode;
    var f = document.createElement;
    var e;

    if(v < 9) {
        ok(!(window instanceof Object), "window instance of Object");
        ok(!(document instanceof Object), "document instance of Object");
        ok(!(f.apply instanceof Function), "f.apply instance of Function");
        ok(!(f.call instanceof Function), "f.call instance of Function");
        ok(!("arguments" in f), "arguments in f");
        ok(!("length" in f), "length in f");
        e = 0;
        try {
            f.toString();
        }catch(ex) {
            e = ex.number;
        }
        ok(e === 0xa01b6 - 0x80000000, "[f.toString] e = " + e);
        try {
            window.toString.call(null);
            ok(false, "expected exception calling window.toString with null context");
        }catch(ex) {}
    }else {
        ok(window instanceof Object, "window not instance of Object");
        ok(document instanceof Object, "document not instance of Object");
        ok(Object.isExtensible(window), "window is not extensible");
        ok(Object.isExtensible(document), "document is not extensible");

        ok(f.toString() === "\nfunction createElement() {\n    [native code]\n}\n", "f.toString() = " + f.toString());
        ok(Object.getPrototypeOf(f) === Function.prototype, "unexpected document.createElement prototype");
        ok(Object.getPrototypeOf(f.apply) === Function.prototype, "unexpected f.apply prototype");
        ok(Object.getPrototypeOf(f.call) === Function.prototype, "unexpected f.call prototype");

        e = window.toString.call(null);
        ok(e === "[object Window]", "window.toString with null context = " + e);
        e = window.toString.call(external.nullDisp);
        ok(e === "[object Window]", "window.toString with nullDisp context = " + e);
    }

    e = 0;
    try {
        f.call(Object, "div");
    }catch(ex) {
        e = ex.number;
    }
    ok(e === (v < 9 ? 0xa0005 : 0x0ffff) - 0x80000000, "[f.call(Object, 'div')] e = " + e);

    e = 0;
    try {
        f.call(null, "div");
    }catch(ex) {
        e = ex.number;
    }
    ok(e === (v < 9 ? 0xa0005 : 0x0ffff) - 0x80000000, "[f.call(null, 'div')] e = " + e);

    var elem = f.call(document, "div");
    elem.setAttribute("class", "cls");
    elem.setAttribute("className", "cls");
    ok(elem.className === "cls", "elem.className = " + elem.className);

    document.body.click.call(elem);

    e = 0;
    try {
        new f();
    }catch(ex) {
        e = ex.number;
    }
    ok(e === (v < 9 ? 0xa01b6 : 0x0ffff) - 0x80000000, "[new f()] e = " + e);

    if(v < 9) {
        ok(!("call" in f.call), "call in f.call");
        ok(!("apply" in f.call), "apply in f.call");
        ok(!("call" in f.apply), "call in f.apply");
        ok(!("apply" in f.apply), "apply in f.apply");
        ok(f.call+"" === "\nfunction call() {\n    [native code]\n}\n", "f.call = " + f.call);
        ok(f.apply+"" === "\nfunction apply() {\n    [native code]\n}\n", "f.apply = " + f.apply);
        ok(external.getVT(f.call) === "VT_DISPATCH", "f.call not VT_DISPATCH");
        ok(external.getVT(f.apply) === "VT_DISPATCH", "f.apply not VT_DISPATCH");

        elem = f.apply(document, ["style"]);
        document.body.appendChild(elem);

        var enumerator = new Enumerator(document.getElementsByTagName("style"));
        enumerator.moveNext();
        var enum_elem = enumerator.item();
        enumerator.moveNext();
        ok(enum_elem === elem, "enum_elem = " + enum_elem);
        ok(enumerator.atEnd(), "enumerator not at end");

        e = 0;
        try {
            f.apply = 0;
        }catch(ex) {
            e = ex.number;
        }
        ok(e === 0xa01b6 - 0x80000000, "[f.apply = 0] e = " + e);
        e = 0;
        try {
            f.call = function() { };
        }catch(ex) {
            e = ex.number;
        }
        ok(e === 0xa01b6 - 0x80000000, "[f.call = function() { }] e = " + e);

        ok(f.apply !== f.apply, "f.apply == f.apply");
        f = f.apply;
        ok(!("arguments" in f), "arguments in f.apply");
        ok(!("length" in f), "length in f.apply");
        ok(!("call" in f), "call in f.apply");
        ok(!("apply" in f), "apply in f.apply");
        e = 0;
        try {
            f.toString();
        }catch(ex) {
            e = ex.number;
        }
        ok(e === 0xa01b6 - 0x80000000, "[f.apply.toString] e = " + e);
        e = 0;
        try {
            f(document, ["style"]);
        }catch(ex) {
            e = ex.number;
        }
        ok(e === 0xa01b6 - 0x80000000, "[f.apply() indirect] e = " + e);
    }else {
        elem = f.call.call(f, document, "div");
        f = f.bind(document);
        elem = f.apply(null, ["style"]);
        document.body.appendChild(elem);

        try {
            var enumerator = new Enumerator(document.getElementsByTagName("style"));
        }catch(ex) {
            e = ex.number;
        }
        ok(e === 0xa01c3 - 0x80000000, "[style Enumerator] e = " + e);

        f.apply = 0;
        f.call = function() { };
        ok(f.apply === 0, "changed f.apply = ", f.apply);
        ok(f.call instanceof Function, "changed f.call not instance of Function");
    }
});

sync_test("builtin_prototypes", function() {
    var v = document.documentMode, r, obj, name, proto;

    function set_obj(n, o) {
        name = n;
        proto = null;
        if(o) {
            proto = window[n]["prototype"];
            if(typeof o !== "boolean") {
                obj = o;
                return;
            }
        }
        try {
            obj = new window[n]();
            ok(o, "expected exception when creating " + name + ".");
        }catch(ex) {
            obj = null;
            ok(!o, "did not expect exception when creating " + name + ".");
            ok(ex.number == 0xa01bd - 0x80000000, "unexpected exception number when creating " + name + ": " + ex.number);
        }
    }
    function test_prop(prop, own) {
        if(own === undefined ? v < 9 : own)
            ok(Object.prototype.hasOwnProperty.call(obj, prop), prop + " not a property of " + name + ".");
        else
            ok(!Object.prototype.hasOwnProperty.call(obj, prop), prop + " is a property of " + name + ".");
        ok(Object.prototype.hasOwnProperty.call(proto, prop), prop + " not a property of " + name + ".prototype.");
    }
    function test_legacy_ctor(methods, props, non_props, set_prop, set_prop_val) {
        if(v >= 9)
            return;
        ok(""+proto === "[Interface prototype object]", name + ".prototype = " + proto);
        if(v < 8)
            ok(proto.constructor === undefined, name + ".prototype.constructor = " + proto.constructor);
        for(var i = 0; i < methods.length; i++) {
            ok(methods[i] in proto, methods[i] + " not in " + name + ".prototype");
            var r = 0;
            try {
                eval("proto." + methods[i] + "();");
            }catch(ex) {
                r = ex.number;
            }
            ok(r === 0xa01b6 - 0x80000000, name + ".prototype." + methods[i] + "() exception code = " + r);
            eval("r = \"\"+proto." + methods[i] + ";");
            ok(r === "\nfunction " + methods[i] + "() {\n    [native code]\n}\n", name + ".prototype." + methods[i] + " = " + r);
            try {
                eval("r = (delete proto." + methods[i] + ");");
                ok(v >= 8, "expected exception deleting " + name + ".prototype." + methods[i]);
                ok(r === true, "delete " + name + ".prototype." + methods[i] + " returned " + r);
            }catch(ex) {
                ok(v < 8, "did not expect exception deleting " + name + ".prototype." + methods[i]);
            }
            eval("r = \"\"+proto." + methods[i] + ";");
            ok(r === "\nfunction " + methods[i] + "() {\n    [native code]\n}\n", name + ".prototype." + methods[i] + " after delete = " + r);
            ok(methods[i] in proto, methods[i] + " not in " + name + ".prototype after delete");

            var func = function() { return "foobar"; }
            eval("proto." + methods[i] + " = func;");
            eval("r = proto." + methods[i] + ";");
            ok(r === func, name + ".prototype." + methods[i] + " after set = " + r);
            try {
                eval("r = (delete proto." + methods[i] + ");");
                ok(v >= 8, "expected exception deleting " + name + ".prototype." + methods[i] + " after set");
                ok(r === true, "delete " + name + ".prototype." + methods[i] + " after set returned " + r);
                eval("r = \"\"+proto." + methods[i] + ";");
                ok(r === "\nfunction " + methods[i] + "() {\n    [native code]\n}\n", name + ".prototype." + methods[i] + " after second delete = " + r);
            }catch(ex) {
                ok(v < 8, "did not expect exception deleting " + name + ".prototype." + methods[i] + " after set");
                eval("r = proto." + methods[i] + ";");
                ok(r === func, name + ".prototype." + methods[i] + " after second delete = " + r);
            }
            eval("proto." + methods[i] + " = func;");
            eval("r = proto." + methods[i] + ";");
            ok(r === func, name + ".prototype." + methods[i] + " after second set = " + r);
        }
        for(var i = 0; i < props.length; i++) {
            ok(props[i] in proto, props[i] + " not in " + name + ".prototype");
            eval("var r = proto." + props[i] + ";");
            ok(r === undefined, name + ".prototype." + props[i] + " = " + r);
            try {
                eval("r = (delete proto." + props[i] + ");");
                ok(v >= 8, "expected exception deleting " + name + ".prototype." + props[i]);
                ok(r === true, "delete " + name + ".prototype." + props[i] + " returned " + r);
            }catch(ex) {
                ok(v < 8, "did not expect exception deleting " + name + ".prototype." + props[i]);
            }
            eval("r = proto." + props[i] + ";");
            ok(r === undefined, name + ".prototype." + props[i] + " after delete = " + r);
            ok(props[i] in proto, props[i] + " not in " + name + ".prototype after delete");
        }
        for(var i = 0; i < non_props.length; i++)
            ok(!(non_props[i] in proto), non_props[i] + " in " + name + ".prototype");

        eval("r = proto." + set_prop + ";");
        ok(r === undefined, name + ".prototype." + set_prop + " = " + r);
        eval("proto." + set_prop + " = set_prop_val; r = proto." + set_prop + ";");
        ok(r === undefined, name + ".prototype." + set_prop + " after set = " + r);

        r = proto.winetestprop;
        ok(r === undefined, name + ".prototype.winetestprop = " + r);
        proto.winetestprop = "test";
        r = proto.winetestprop;
        ok(r === "test", name + ".prototype.winetestprop after set = " + r);
    }

    set_obj("XMLHttpRequest", true);
    test_prop("open");
    test_prop("status");
    test_prop("onreadystatechange");
    test_legacy_ctor(["abort", "send"], ["readyState", "status"], ["selected", "src", "getAttribute"], "onreadystatechange", function(){});
    if(v < 9) {
        r = obj.abort();
        ok(r === "foobar", "(new XMLHttpRequest).abort() returned " + r);
        r = obj.winetestprop;
        ok(r === "test", "(new XMLHttpRequest).winetestprop = " + r);
        obj.winetestprop = "prop";
        r = obj.winetestprop;
        ok(r === "prop", "(new XMLHttpRequest).winetestprop after set = " + r);
        r = XMLHttpRequest.prototype.winetestprop;
        ok(r === "test", "XMLHttpRequest.prototype.winetestprop after obj = " + r);
    }else
        ok(proto.constructor === window.XMLHttpRequest, "XMLHttpRequest.prototype.constructor = " + proto.constructor);

    set_obj("Image", true);
    test_prop("src");
    test_prop("border");
    test_legacy_ctor(["getAttribute", "toString"], ["isMap", "alt"], ["selected", "send"], "src", "about:blank");
    if(v < 9) {
        r = obj.toString();
        ok(r === "foobar", "(new Image).toString() returned " + r);
        r = obj.winetestprop;
        ok(r === "test", "(new Image).winetestprop = " + r);
        obj.winetestprop = "prop";
        r = obj.winetestprop;
        ok(r === "prop", "(new Image).winetestprop after set = " + r);
        r = window.Image.prototype.winetestprop;
        ok(r === "test", "Image.prototype.winetestprop after obj = " + r);
        try {
            r = (delete obj.winetestprop);
            ok(v >= 8, "expected exception deleting (new Image).winetestprop");
            ok(r === true, "delete (new Image).winetestprop returned " + r);
        }catch(ex) {
            ok(v < 8, "did not expect exception deleting (new Image).winetestprop");
        }
        r = obj.winetestprop;
        ok(r === (v < 8 ? "prop" : "test"), "(new Image).winetestprop after delete = " + r);
        obj = new window.Image();
        r = obj.winetestprop;
        ok(r === "test", "(new Image).winetestprop second time = " + r);
        window.Image.prototype.winetestprop = "string";
        r = obj.winetestprop;
        ok(r === "string", "(new Image).winetestprop after change in prototype = " + r);
    }else
        ok(proto.constructor === window.HTMLImageElement, "Image.prototype.constructor = " + proto.constructor);

    set_obj("Option", true);
    test_prop("text");
    test_prop("selected");
    test_legacy_ctor(["setAttribute", "contains"], ["index", "value"], ["src", "send"], "text", "foo");
    if(v < 9) {
        r = obj.setAttribute("a", "b");
        ok(r === "foobar", "(new Option).setAttribute() returned " + r);
        r = obj.winetestprop;
        ok(r === "test", "(new Option).winetestprop = " + r);
        obj.winetestprop = "prop";
        r = obj.winetestprop;
        ok(r === "prop", "(new Option).winetestprop after set = " + r);
        r = window.Option.prototype.winetestprop;
        ok(r === "test", "Option.prototype.winetestprop after obj = " + r);
        try {
            r = (delete obj.winetestprop);
            ok(v >= 8, "expected exception deleting (new Option).winetestprop");
            ok(r === true, "delete (new Option).winetestprop returned " + r);
        }catch(ex) {
            ok(v < 8, "did not expect exception deleting (new Option).winetestprop");
        }
        r = obj.winetestprop;
        ok(r === (v < 8 ? "prop" : "test"), "(new Option).winetestprop after delete = " + r);
        obj = new window.Option();
        r = obj.winetestprop;
        ok(r === "test", "(new Option).winetestprop second time = " + r);
        window.Option.prototype.winetestprop = "string";
        r = obj.winetestprop;
        ok(r === "string", "(new Option).winetestprop after change in prototype = " + r);
    }else
        ok(proto.constructor === window.HTMLOptionElement, "Option.prototype.constructor = " + proto.constructor);

    // other constructors don't support construction
    set_obj("ClientRect");
    set_obj("ClientRectList");
    set_obj("Console");
    set_obj("CustomEvent");
    set_obj("DOMTokenList");
    set_obj("KeyboardEvent");
    set_obj("MessageEvent");
    set_obj("MouseEvent");
    set_obj("MSCSSRuleList");
    set_obj("MSCurrentStyleCSSProperties");
    set_obj("MSEventObj");
    set_obj("MSNamespaceInfoCollection");
    set_obj("MSSelection");
    set_obj("MSStyleCSSProperties");
    set_obj("Performance");
    set_obj("PerformanceNavigation");
    set_obj("PerformanceTiming");
    set_obj("UIEvent");
    if(v >= 9) {
        set_obj("Attr");
        set_obj("CSSStyleDeclaration");
        set_obj("CSSStyleRule");
        set_obj("CSSStyleSheet");
        set_obj("DOMImplementation");
        set_obj("Event");
        set_obj("History");
        set_obj("HTMLCollection");
        set_obj("NamedNodeMap");
        set_obj("Navigator");
        set_obj("NodeList");
        set_obj("Screen");
        set_obj("Storage");
        set_obj("StyleSheetList");
        set_obj("Text");
        set_obj("TextRange");
        set_obj("Window");
    }
    if(v >= 11) {
        set_obj("Crypto");
    }

    if(v >= 8 && v < 11) {
        set_obj(v < 9 ? "Event" : "MSEventObj", document.createEventObject());
        test_prop("x");
        test_prop("y");
        test_prop("srcElement");
        test_prop("returnValue");

        if(Object.create) {
            obj = Object.create(proto);
            test_prop("reason");
            test_prop("srcFilter");
            r = Object.prototype.toString.call(obj);
            ok(r === "[object Object]", "Object.toString on obj created from MSEventObj.prototype returned " + r);
        }

        var ctor = function() {};
        ctor.prototype = proto;
        ctor.prototype.testWineProp = function() { return 42; };
        obj = new ctor();
        test_prop("shiftKey", false);
        test_prop("testWineProp", false);
        r = Object.prototype.toString.call(obj);
        ok(r === "[object Object]", "Object.toString on custom obj returned " + r);

        r = (delete proto.shiftKey);
        ok(r === true, "delete shiftKey returned " + r);
        if(v < 9)
            ok(Object.prototype.hasOwnProperty.call(proto, "shiftKey"), "shiftKey not a property anymore of Event.prototype.");
        else {
            ok(!Object.prototype.hasOwnProperty.call(proto, "shiftKey"), "shiftKey still a property of MSEventObj.prototype.");
            proto.shiftKey = ctor;
            ok(proto.shiftKey === ctor, "shiftKey = " + proto.shiftKey);
        }

        r = (delete proto.testWineProp);
        ok(r === true, "delete testWineProp returned " + r);
        ok(!Object.prototype.hasOwnProperty.call(proto, "testWineProp"), "testWineProp still a property of " + name + ".prototype.");
    }

    if(v >= 9) {
        set_obj("Event", document.createEvent("Event"));
        test_prop("initEvent");
        test_prop("currentTarget");

        obj = Object.create(proto);
        test_prop("eventPhase");
        test_prop("preventDefault");
        r = Object.prototype.toString.call(obj);
        ok(r === "[object Object]", "Object.toString on obj created from Event.prototype returned " + r);

        var ctor = function() {};
        ctor.prototype = proto;
        ctor.prototype.testWineProp = function() { return 42; };
        obj = new ctor();
        test_prop("timeStamp");
        test_prop("testWineProp");
        r = Object.prototype.toString.call(obj);
        ok(r === "[object Object]", "Object.toString on custom obj returned " + r);

        r = (delete proto.timeStamp);
        ok(r === true, "delete timeStamp returned " + r);
        ok(!Object.prototype.hasOwnProperty.call(proto, "timeStamp"), "timeStamp still a property of Event.prototype.");

        r = (delete proto.testWineProp);
        ok(r === true, "delete testWineProp returned " + r);
        ok(!Object.prototype.hasOwnProperty.call(proto, "testWineProp"), "testWineProp still a property of Event.prototype.");

        proto.timeStamp = ctor;
        ok(proto.timeStamp === ctor, "timeStamp = " + proto.timeStamp);

        set_obj("HTMLImageElement", document.createElement("img"));
        document.body.setAttribute.call(obj, "width", "100");
        obj = Object.create(proto);
        r = 0;
        try {
            document.body.setAttribute.call(obj, "width", "100");
        }catch(ex) {
            r = ex.number;
        }
        ok(r === 0xffff - 0x80000000, "document.body.setAttribute.call(obj ...) exception code = " + r);
    }

    if(v >= 8) {
        obj = window.HTMLMetaElement;
        ok(!("charset" in obj), "charset in HTMLMetaElement constructor.");
        ok(!("setAttribute" in obj), "setAttribute in HTMLMetaElement constructor.");
        ok(!Object.prototype.hasOwnProperty.call(obj, "charset"), "charset is a property of HTMLMetaElement constructor.");
        if(Object.getPrototypeOf)
            ok(Object.getPrototypeOf(obj) === Object.prototype, "getPrototypeOf(HTMLMetaElement constructor) = " + Object.getPrototypeOf(obj));
        r = 0;
        try {
            document.body.setAttribute.call(obj, "charset", "UTF-8");
        }catch(ex) {
            r = ex.number;
        }
        ok(r === (v < 9 ? 0xa0005 : 0xffff) - 0x80000000, "setAttribute on HTMLMetaElement constructor error code = " + r);

        proto = window.HTMLMetaElement.prototype;
        try {
            window.HTMLMetaElement.prototype = Object.prototype;
            ok(v >= 9, "expected exception setting HTMLMetaElement.prototype");
        }catch(ex) {
            ok(v < 9, "did not expect exception setting HTMLMetaElement.prototype");
            ok(ex.number === 0xa01b6 - 0x80000000, "exception code setting HTMLMetaElement.prototype = " + ex.number);
        }
        ok(window.HTMLMetaElement.prototype === proto, "HTMLMetaElement.prototype = " + window.HTMLMetaElement.prototype);
        ok(proto !== Object.prototype, "old prototype is Object.prototype");

        obj = document.createElement("meta");
        ok("tagName" in obj, "tagName not in HTMLMetaElement");
        if(Object.getPrototypeOf)
            ok(Object.getPrototypeOf(obj) === proto, "getPrototypeOf(meta element) = " + Object.getPrototypeOf(obj));

        try {
            r = (delete window.HTMLMetaElement.prototype);
            ok(r === false, "delete HTMLMetaElement.prototype returned " + r);
            ok(v >= 9, "expected exception deleting HTMLMetaElement.prototype");
        }catch(ex) {
            ok(v < 9, "did not expect exception deleting HTMLMetaElement.prototype");
            ok(ex.number === 0xa01b6 - 0x80000000, "exception code deleting HTMLMetaElement.prototype = " + ex.number);
        }
        ok(Object.prototype.hasOwnProperty.call(window.HTMLMetaElement, "prototype"), "prototype not a property anymore of HTMLMetaElement.");

        try {
            r = (delete window.HTMLMetaElement);
            ok(r === true, "delete HTMLMetaElement returned " + r);
            ok(v >= 9, "expected exception deleting HTMLMetaElement");
            ok(!Object.prototype.hasOwnProperty.call(window, "HTMLMetaElement"), "HTMLMetaElement still a property of window.");
        }catch(ex) {
            ok(v < 9, "did not expect exception deleting HTMLMetaElement");
            ok(ex.number === 0xa01bd - 0x80000000, "exception code deleting HTMLMetaElement = " + ex.number);
            ok(Object.prototype.hasOwnProperty.call(window, "HTMLMetaElement"), "HTMLMetaElement not a property anymore of window.");
        }

        obj = document.createElement("meta");
        ok("tagName" in obj, "tagName not in HTMLMetaElement");
        if(Object.getPrototypeOf) {
            ok(Object.getPrototypeOf(obj) === proto, "getPrototypeOf(meta element) = " + Object.getPrototypeOf(obj));
            ok(window.HTMLMetaElement === undefined, "HTMLMetaElement = " + window.HTMLMetaElement);
        }

        ok("setAttribute" in proto, "setAttribute not in proto.");
        r = 0;
        try {
            obj.setAttribute.call(proto, "charset", "UTF-8");
        }catch(ex) {
            r = ex.number;
        }
        ok(r === (v < 9 ? 0xa01b6 : 0xffff) - 0x80000000, "setAttribute on proto error code = " + r);
        r = 0;
        try {
            proto.setAttribute("charset", "UTF-8");
        }catch(ex) {
            r = ex.number;
        }
        ok(r === (v < 9 ? 0xa01b6 : 0xffff) - 0x80000000, "proto.setAttribute error code = " + r);

        ok(Object.prototype.hasOwnProperty.call(proto, "charset"), "charset not a property of proto.");
        if(v < 9) {
            proto.charset = "UTF-8";
            ok(proto.charset === undefined, "proto.charset = " + proto.charset);
        }else {
            r = Object.getOwnPropertyDescriptor(proto, "charset");
            ok(r.get.toString() === "\nfunction charset() {\n    [native code]\n}\n", "charset.get = " + r.get.toString());
            ok(r.set.toString() === "\nfunction charset() {\n    [native code]\n}\n", "charset.set = " + r.set.toString());
            ok(Object.getPrototypeOf(r.get) === Function.prototype, "unexpected charset.get prototype");
            ok(Object.getPrototypeOf(r.set) === Function.prototype, "unexpected charset.set prototype");

            r = 0;
            try {
                proto.charset;
            }catch(ex) {
                r = ex.number;
            }
            ok(r === 0xffff - 0x80000000, "proto.charset error code = " + r);
            r = 0;
            try {
                proto.charset = "UTF-8";
            }catch(ex) {
                r = ex.number;
            }
            ok(r === 0xffff - 0x80000000, "set proto.charset error code = " + r);
        }
    }

    if(v >= 9) {
        var protos = [
            [ "CharacterData",                  "Node" ],
            [ "ClientRect",                     "Object" ],
            [ "ClientRectList",                 "Object" ],
            [ "Comment",                        "CharacterData" ],
            [ "Console",                        "Object" ],
            [ "Crypto",                         "Object" ],
            [ "CSSRule",                        "Object" ],
            [ "CSSStyleDeclaration",            "Object" ],
            [ "CSSStyleRule",                   "CSSRule" ],
            [ "CSSStyleSheet",                  "StyleSheet" ],
            [ "CustomEvent",                    "Event" ],
            [ "Document",                       "Node" ],
            [ "DocumentType",                   "Node" ],
            [ "DOMImplementation",              "Object" ],
            [ "DOMTokenList",                   "Object" ],
            [ "Element",                        "Node" ],
            [ "Event",                          "Object" ],
            [ "History",                        "Object" ],
            [ "HTMLAnchorElement",              "HTMLElement" ],
            [ "HTMLAreaElement",                "HTMLElement" ],
            [ "HTMLBodyElement",                "HTMLElement" ],
            [ "HTMLButtonElement",              "HTMLElement" ],
            [ "HTMLCollection",                 "Object" ],
            [ "HTMLDocument",                   "Document" ],
            [ "HTMLElement",                    "Element" ],
            [ "HTMLEmbedElement",               "HTMLElement" ],
            [ "HTMLFormElement",                "HTMLElement" ],
            [ "HTMLFrameElement",               "HTMLElement" ],
            [ "HTMLHeadElement",                "HTMLElement" ],
            [ "HTMLHtmlElement",                "HTMLElement" ],
            [ "HTMLIFrameElement",              "HTMLElement" ],
            [ "HTMLImgElement",                 "HTMLElement" ],
            [ "HTMLInputElement",               "HTMLElement" ],
            [ "HTMLLabelElement",               "HTMLElement" ],
            [ "HTMLLinkElement",                "HTMLElement" ],
            [ "HTMLMetaElement",                "HTMLElement" ],
            [ "HTMLObjectElement",              "HTMLElement" ],
            [ "HTMLOptionElement",              "HTMLElement" ],
            [ "HTMLScriptElement",              "HTMLElement" ],
            [ "HTMLSelectElement",              "HTMLElement" ],
            [ "HTMLStyleElement",               "HTMLElement" ],
            [ "HTMLTableCellElement",           "HTMLElement" ],
            [ "HTMLTableDataCellElement",       "HTMLTableCellElement" ],
            [ "HTMLTableElement",               "HTMLElement" ],
            [ "HTMLTableRowElement",            "HTMLElement" ],
            [ "HTMLTextAreaElement",            "HTMLElement" ],
            [ "HTMLTitleElement",               "HTMLElement" ],
            [ "HTMLUnknownElement",             "HTMLElement" ],
            [ "Image",                          "HTMLElement" ],
            [ "KeyboardEvent",                  "UIEvent" ],
            [ "MediaQueryList",                 "Object" ],
            [ "MessageEvent",                   "Event" ],
            [ "MimeTypeArray",                  "Object" ],
            [ "MouseEvent",                     "UIEvent" ],
            [ "MSCSSProperties",                "CSSStyleDeclaration" ],
            [ "MSCSSRuleList",                  "Object" ],
            [ "MSCurrentStyleCSSProperties",    "MSCSSProperties" ],
            [ "MSEventObj",                     "Object" ],
            [ "MSMimeTypesCollection",          "Object" ],
            [ "MSNamespaceInfoCollection",      "Object" ],
            [ "MSPluginsCollection",            "Object" ],
            [ "MSSelection",                    "Object" ],
            [ "MSStyleCSSProperties",           "MSCSSProperties" ],
            [ "MutationObserver",               "Object" ],
            [ "NamedNodeMap",                   "Object" ],
            [ "Navigator",                      "Object" ],
            [ "Node",                           "Object" ],
            [ "NodeList",                       "Object" ],
            [ "Option",                         "HTMLElement" ],
            [ "PageTransitionEvent",            "Event" ],
            [ "Performance",                    "Object" ],
            [ "PerformanceNavigation",          "Object" ],
            [ "PerformanceTiming",              "Object" ],
            [ "PluginArray",                    "Object" ],
            [ "ProgressEvent",                  "Event" ],
            [ "Screen",                         "Object" ],
            [ "Storage",                        "Object" ],
            [ "StorageEvent",                   "Event" ],
            [ "StyleSheet",                     "Object" ],
            [ "StyleSheetList",                 "Object" ],
            [ "Text",                           "CharacterData" ],
            [ "TextRange",                      "Object" ],
            [ "UIEvent",                        "Event" ],
            [ "Window",                         "Object" ],
            [ "XMLHttpRequest",                 "Object" ]
        ];

        for(var i = 0; i < protos.length; i++) {
            if(!(protos[i][0] in window))
                continue;
            var a, b;
            eval("a = Object.getPrototypeOf(" + protos[i][0] + ".prototype); b = " + protos[i][1] + ".prototype;");
            ok(a === b, "getPrototypeOf(" + protos[i][0] + ".prototype) = " + a);
        }

        var CSS_props = [ "accelerator","backgroundPositionX","backgroundPositionY","getAttribute","imeMode","layoutFlow","layoutGrid","layoutGridChar",
                          "layoutGridLine","layoutGridMode","layoutGridType","lineBreak","msBlockProgression","msInterpolationMode","removeAttribute",
                          "scrollbar3dLightColor","scrollbarArrowColor","scrollbarBaseColor","scrollbarDarkShadowColor","scrollbarFaceColor",
                          "scrollbarHighlightColor","scrollbarShadowColor","scrollbarTrackColor","setAttribute","styleFloat","textAutospace",
                          "textJustifyTrim","textKashida","textKashidaSpace","writingMode","zoom" ];
        var Elem_props = [ "clientHeight","clientLeft","clientTop","clientWidth","firstElementChild","getAttribute","getAttributeNode","getAttributeNodeNS",
                           "getAttributeNS","getBoundingClientRect","getClientRects","getElementsByTagName","getElementsByTagNameNS","hasAttribute",
                           "hasAttributeNS","lastElementChild","msMatchesSelector","nextElementSibling","previousElementSibling","querySelector",
                           "removeAttribute","removeAttributeNode","removeAttributeNS","scrollHeight","scrollLeft","scrollTop","scrollWidth","setAttribute",
                           "setAttributeNode","setAttributeNodeNS","setAttributeNS","tagName" ];
        var Event_props = [ "bubbles","cancelable","cancelBubble","currentTarget","defaultPrevented","eventPhase","initEvent","isTrusted",
                            "preventDefault","srcElement","stopImmediatePropagation","stopPropagation","target","timeStamp","type" ];
        var HtmlElem_props = [ "accessKey","applyElement","blur","canHaveHTML","children","className","clearAttributes","click","componentFromPoint",
                               "contains","contentEditable","createControlRange","currentStyle","dir","disabled","dragDrop","focus","getAdjacentText",
                               "getElementsByClassName","hideFocus","id","innerHTML","innerText","insertAdjacentElement","insertAdjacentHTML",
                               "insertAdjacentText","isContentEditable","isDisabled","isMultiLine","isTextEdit","lang","language","mergeAttributes",
                               "offsetHeight","offsetLeft","offsetParent","offsetTop","offsetWidth","onabort","onactivate","onbeforeactivate","onbeforecopy",
                               "onbeforecut","onbeforedeactivate","onbeforepaste","onblur","oncanplay","oncanplaythrough","onchange","onclick",
                               "oncontextmenu","oncopy","oncut","ondblclick","ondeactivate","ondrag","ondragend","ondragenter","ondragleave","ondragover",
                               "ondragstart","ondrop","ondurationchange","onemptied","onended","onerror","onfocus","onfocusin","onfocusout","onhelp",
                               "oninput","onkeydown","onkeypress","onkeyup","onload","onloadeddata","onloadedmetadata","onloadstart","onmousedown",
                               "onmouseleave","onmousemove","onmouseout","onmouseover","onmouseup","onmousewheel","onpaste","onpause","onplay","onplaying",
                               "onprogress","onratechange","onreset","onscroll","onseeked","onseeking","onselect","onselectstart","onstalled","onsubmit",
                               "onsuspend","ontimeupdate","onvolumechange","onwaiting","outerHTML","outerText","parentElement","parentTextEdit",
                               "recordNumber","releaseCapture","replaceAdjacentText","runtimeStyle","scrollIntoView","setActive","setCapture","sourceIndex",
                               "style","tabIndex","title","uniqueID","uniqueNumber" ];
        var Node_props = [ "addEventListener","appendChild","attributes","childNodes","cloneNode","compareDocumentPosition","dispatchEvent","firstChild",
                           "hasChildNodes","insertBefore","isDefaultNamespace","isEqualNode","isSameNode","isSupported","lastChild","localName",
                           "lookupNamespaceURI","lookupPrefix","namespaceURI","nextSibling","nodeName","nodeType","nodeValue","ownerDocument",
                           "parentNode","prefix","previousSibling","removeChild","removeEventListener","replaceChild","textContent" ];
        var TableCell_props = [ "align","background","bgColor","borderColor","borderColorDark","borderColorLight","cellIndex","colSpan","height","noWrap",
                                "rowSpan","vAlign","width" ];

        protos = [
            [ "CharacterData", ["data","length","appendData"], Node_props ],
            [ "Comment", ["text"], ["insertData","replaceData","substringData"] ],
            [ "CSSStyleRule", ["readOnly","selectorText","style"], ["cssText","parentRule","parentStyleSheet","type" ] ],
            [ "CSSStyleSheet", ["addRule","cssRules","ownerRule","rules"], ["disabled","media","ownerNode","parentStyleSheet","title","type"] ],
            [ "CustomEvent", ["detail","initCustomEvent"], Event_props ],
            [ "Document", ["body","doctype","documentMode","onactivate","parentWindow","styleSheets","title"], Node_props ],
            [ "DocumentType", ["entities","internalSubset","name","notations","publicId","systemId"], Node_props ],
            [ "Element", Elem_props, Node_props ],
            [ "HTMLElement", HtmlElem_props, Elem_props ],
            [ "HTMLTableCellElement", TableCell_props, HtmlElem_props ],
            [ "HTMLTableDataCellElement", [], TableCell_props ],
            [ "HTMLUnknownElement", ["recordset","namedRecordset"], HtmlElem_props ],
            [ "KeyboardEvent", ["altKey","ctrlKey","getModifierState","initKeyboardEvent","key","metaKey"], ["detail","initUIEvent","view"] ],
            [ "MessageEvent", ["data","initMessageEvent","origin","source"], Event_props ],
            [ "MouseEvent", ["button","clientX","initMouseEvent","offsetY","pageX","shiftKey","x","y"], ["detail","initUIEvent","view"] ],
            [ "MSCSSProperties", CSS_props, ["background","border","clip","fontWeight","listStyle","quotes","setProperty","zIndex"] ],
            [ "MSCurrentStyleCSSProperties", ["blockDirection","clipBottom","clipLeft","clipRight","clipTop","hasLayout"], CSS_props ],
            [ "MSStyleCSSProperties", ["pixelTop","pixelWidth","posHeight","posLeft","textDecorationBlink","textDecorationNone"], CSS_props ],
            [ "ProgressEvent", ["initProgressEvent","lengthComputable","loaded","total"], Event_props ],
            [ "StorageEvent", ["initStorageEvent","key","newValue","oldValue","storageArea"], Event_props ],
            [ "Text", ["splitText"], ["data","length","appendData","deleteData","insertData","replaceData","substringData"] ],
            [ "UIEvent", ["detail","initUIEvent","view"], Event_props ]
        ];

        for(var i = 0; i < protos.length; i++) {
            if(!(protos[i][0] in window))
                continue;
            eval("r = " + protos[i][0] + ".prototype");
            for(var j = 0; j < protos[i][1].length; j++)
                ok(Object.prototype.hasOwnProperty.call(r, protos[i][1][j]), protos[i][1][j] + " not a property of " + protos[i][0] + ".prototype");
            for(var j = 0; j < protos[i][2].length; j++) {
                ok(!Object.prototype.hasOwnProperty.call(r, protos[i][2][j]), protos[i][2][j] + " is a property of " + protos[i][0] + ".prototype");
                ok(protos[i][2][j] in r, protos[i][2][j] + " not in " + protos[i][0] + ".prototype");
            }
        }
    }
});

sync_test("builtin_constructors", function() {
    var v = document.documentMode;

    var special_ctors = [
        [ "Image",              [ "prototype", "arguments" ], [ "create", "length" ] ],
        [ "MutationObserver",   [ "prototype", "arguments" ], [ "create", "length" ], 11 ],
        [ "Option",             [ "prototype", "arguments" ], [ "create", "length" ] ],
        [ "XMLHttpRequest",     [ "prototype", "arguments", "create" ], [ "length" ] ]
    ];
    for(var i = 0; i < special_ctors.length; i++) {
        if(special_ctors[i].length > 3 && v < special_ctors[i][3])
            continue;
        var name = special_ctors[i][0];
        ok(Object.prototype.hasOwnProperty.call(window, name), name + " not a property of window.");
        var obj = window[name];
        if(v < 9) {
            ok(!Object.prototype.hasOwnProperty.call(obj, "arguments"), "arguments is a property of " + name + " constructor.");
            ok(Object.prototype.hasOwnProperty.call(obj, "create"), "create not a property of " + name + " constructor.");
            ok(!Object.prototype.hasOwnProperty.call(obj, "length"), "length is a property of " + name + " constructor.");
            ok(Object.prototype.hasOwnProperty.call(obj, "prototype"), "prototype not a property of " + name + " constructor.");
            ok(!("length" in obj), "length in " + name + " constructor.");
            if(window.Window)
                ok(!Object.prototype.hasOwnProperty.call(window.Window.prototype, name), name + " is a property of window's prototype.");
        }else {
            for(var j = 0; j < special_ctors[i][1].length; j++)
                ok(Object.prototype.hasOwnProperty.call(obj, special_ctors[i][1][j]), special_ctors[i][1][j] + " not a property of " + name + " constructor.");

            for(var j = 0; j < special_ctors[i][2].length; j++)
                ok(!Object.prototype.hasOwnProperty.call(obj, special_ctors[i][2][j]), special_ctors[i][2][j] + " is a property of " + name + " constructor.");

            ok(Object.getPrototypeOf(obj) === Function.prototype, "getPrototypeOf(" + name + " constructor) = " + Object.getPrototypeOf(obj));
            ok(!Object.prototype.hasOwnProperty.call(Object.getPrototypeOf(window), name), name + " is a property of window's prototype.");

            if(obj.create) {
                var proto = obj.prototype, func = obj.create, s = Object.prototype.toString.call(func);
                ok(s === "[object Function]", "obj.create toString = " + s);
                ok(Object.getPrototypeOf(func) === Function.prototype, "getPrototypeOf(" + name + ".create) = " + Object.getPrototypeOf(func));
                ok(Object.prototype.hasOwnProperty.call(func, "arguments"), "arguments not a property of " + name + ".create");
                ok(!Object.prototype.hasOwnProperty.call(func, "length"), "length is a property of " + name + ".create");
                ok(Object.prototype.hasOwnProperty.call(func, "prototype"), "prototype not a property of " + name + ".create");

                obj = func();
                ok(Object.getPrototypeOf(obj) === proto, "getPrototypeOf(obj.create()) = " + Object.getPrototypeOf(obj));
                obj = func.call(Object);
                ok(Object.getPrototypeOf(obj) === proto, "getPrototypeOf(obj.create() on Object) = " + Object.getPrototypeOf(obj));
            }
        }
    }

    if(v < 9) {
        // IHTMLDOMConstructorCollection props
        var ctors = [
            [ "Attr" ],
            [ "BehaviorUrnsCollection" ],
            [ "BookmarkCollection" ],
            [ "CSSCurrentStyleDeclaration" ],
            [ "CSSRuleList" ],
            [ "CSSRuleStyleDeclaration" ],
            [ "CSSStyleDeclaration" ],
            [ "CSSStyleRule" ],
            [ "CSSStyleSheet" ],
            [ "CompatibleInfo" ],
            [ "CompatibleInfoCollection" ],
            [ "ControlRangeCollection" ],
            [ "DOMImplementation" ],
            [ "DataTransfer" ],
            [ "Element" ],
            [ "Event" ],
            [ "HTCElementBehaviorDefaults" ],
            [ "HTMLAnchorElement" ],
            [ "HTMLAreaElement" ],
            [ "HTMLAreasCollection" ],
            [ "HTMLBGSoundElement" ],
            [ "HTMLBRElement" ],
            [ "HTMLBaseElement" ],
            [ "HTMLBaseFontElement" ],
            [ "HTMLBlockElement" ],
            [ "HTMLBodyElement" ],
            [ "HTMLButtonElement" ],
            [ "HTMLCollection" ],
            [ "HTMLCommentElement" ],
            [ "HTMLDDElement" ],
            [ "HTMLDListElement" ],
            [ "HTMLDTElement" ],
            [ "HTMLDivElement" ],
            [ "HTMLDocument" ],
            [ "HTMLEmbedElement" ],
            [ "HTMLFieldSetElement" ],
            [ "HTMLFontElement" ],
            [ "HTMLFormElement" ],
            [ "HTMLFrameElement" ],
            [ "HTMLFrameSetElement" ],
            [ "HTMLGenericElement" ],
            [ "HTMLHRElement" ],
            [ "HTMLHeadElement" ],
            [ "HTMLHeadingElement" ],
            [ "HTMLHtmlElement" ],
            [ "HTMLIFrameElement" ],
            [ "HTMLImageElement" ],
            [ "HTMLInputElement" ],
            [ "HTMLIsIndexElement" ],
            [ "HTMLLIElement" ],
            [ "HTMLLabelElement" ],
            [ "HTMLLegendElement" ],
            [ "HTMLLinkElement" ],
            [ "HTMLMapElement" ],
            [ "HTMLMarqueeElement" ],
            [ "HTMLMetaElement" ],
            [ "HTMLModelessDialog" ],
            [ "HTMLNamespaceInfo" ],
            [ "HTMLNamespaceInfoCollection" ],
            [ "HTMLNextIdElement" ],
            [ "HTMLNoShowElement" ],
            [ "HTMLOListElement" ],
            [ "HTMLObjectElement" ],
            [ "HTMLOptionElement" ],
            [ "HTMLParagraphElement" ],
            [ "HTMLParamElement" ],
            [ "HTMLPhraseElement" ],
            [ "HTMLPluginsCollection" ],
            [ "HTMLPopup" ],
            [ "HTMLScriptElement" ],
            [ "HTMLSelectElement" ],
            [ "HTMLSpanElement" ],
            [ "HTMLStyleElement" ],
            [ "HTMLTableCaptionElement" ],
            [ "HTMLTableCellElement" ],
            [ "HTMLTableColElement" ],
            [ "HTMLTableElement" ],
            [ "HTMLTableRowElement" ],
            [ "HTMLTableSectionElement" ],
            [ "HTMLTextAreaElement" ],
            [ "HTMLTextElement" ],
            [ "HTMLTitleElement" ],
            [ "HTMLUListElement" ],
            [ "HTMLUnknownElement" ],
            [ "History" ],
            [ "Image", 0, "HTMLImageElement" ],
            [ "Location" ],
            [ "NamedNodeMap" ],
            [ "Navigator" ],
            [ "NodeList" ],
            [ "Option", 0, "HTMLOptionElement" ],
            [ "Screen" ],
            [ "Selection" ],
            [ "StaticNodeList" ],
            [ "Storage" ],
            [ "StyleSheetList" ],
            [ "StyleSheetPage" ],
            [ "StyleSheetPageList" ],
            [ "Text" ],
            [ "TextRange" ],
            [ "TextRangeCollection" ],
            [ "TextRectangle" ],
            [ "TextRectangleList" ],
            [ "Window" ],
            [ "XDomainRequest", 0 ],
            [ "XMLHttpRequest", 0 ]
        ];
        for(var i = 0; i < ctors.length; i++) {
            if(!(ctors[i][0] in window) && (v >= 8 || ctors[i][0] === "XDomainRequest")) {
                todo_wine.ok(false, ctors[i][0] + " not implemented");
                continue;
            }
            var a, b, r = 0;
            try {
                eval("a = " + ctors[i][0] + "; b = window." + ctors[i][0] + ";");
            }catch(ex) {
                r = ex.number;
            }
            if(r === 0x4001 - 0x80000000)  /* todo_wine XDomainRequest */
                continue;
            if(v < 8 && (ctors[i].length < 2 || v < ctors[i][1]))
                ok(r === 0xa1391 - 0x80000000, ctors[i][0] + " not undefined: " + r);
            else {
                ok(r === 0, ctors[i][0] + " exception code: " + r);
                ok(a === b, ctors[i][0] + ": " + a + " != " + b);
                ok(ctors[i][0] in window, ctors[i][0] + " in window");
                if(v >= 8)
                    ok(!(ctors[i][0] in window.Window.prototype), ctors[i][0] + " in Window.prototype");
                r = "" + a;
                ok(r === "[object " + ctors[i][ctors[i].length < 3 ? 0 : 2] + "]", ctors[i][0] + " returned " + r);
                r = "" + a.prototype;
                ok(r === "[Interface prototype object]", ctors[i][0] + ".prototype returned " + r);

                var props = [ "LookupGetter", "LookupSetter", "DefineGetter", "DefineSetter" ];
                for(var j = 0; j < props.length; j++) {
                    ok(!(props[j] in a.prototype), props[j] + " in " + ctors[i][0] + ".prototype");
                    ok(!(props[j] in a), props[j] + " in " + ctors[i][0]);
                }
                ok(!("constructor" in a), "constructor in " + ctors[i][0]);

                if(v < 8 || ctors[i][0] === "HTMLModelessDialog") {
                    ok(!("constructor" in a.prototype), "constructor in " + ctors[i][0] + ".prototype");
                }else {
                    ok("constructor" in a.prototype, "constructor not in " + ctors[i][0] + ".prototype");
                    b = a.prototype.constructor;
                    r = ctors[i][(ctors[i].length > 2) ? 2 : 0];
                    var ctor = (r.length > 7 && r.slice(-7) === "Element") ? window.Element : null;
                    ok(b === ctor, ctors[i][0] + ".prototype.constructor = " + b);
                    a.prototype.constructor = "foobar";
                    b = a.prototype.constructor;
                    ok(b === ctor, ctors[i][0] + ".prototype.constructor after set = " + b);
                    r = (delete a.prototype.constructor);
                    ok(r === true, "delete " + ctors[i][0] + ".prototype.constructor returned " + r);
                    b = a.prototype.constructor;
                    ok(b === ctor, ctors[i][0] + ".prototype.constructor after delete = " + b);
                }
            }
        }
    }else {
        var ctors = [
            [ "Attr" ],
            [ "CharacterData" ],
            [ "ClientRect" ],
            [ "ClientRectList" ],
            [ "Comment" ],
            [ "Console", 10 ],
            [ "Crypto", 11 ],
            [ "CSSRule" ],
            [ "CSSStyleDeclaration" ],
            [ "CSSStyleRule" ],
            [ "CSSStyleSheet" ],
            [ "CustomEvent" ],
            [ "Document" ],
            [ "DocumentType" ],
            [ "DOMImplementation" ],
            [ "DOMTokenList", 10 ],
            [ "Element" ],
            [ "Event" ],
            [ "History" ],
            [ "HTMLAnchorElement" ],
            [ "HTMLAreaElement" ],
            [ "HTMLBodyElement" ],
            [ "HTMLButtonElement" ],
            [ "HTMLCollection" ],
            [ "HTMLDocument", 11 ],
            [ "HTMLElement" ],
            [ "HTMLEmbedElement" ],
            [ "HTMLFormElement" ],
            [ "HTMLFrameElement" ],
            [ "HTMLHeadElement" ],
            [ "HTMLHtmlElement" ],
            [ "HTMLIFrameElement" ],
            [ "HTMLImageElement" ],
            [ "HTMLInputElement" ],
            [ "HTMLLabelElement" ],
            [ "HTMLLinkElement" ],
            [ "HTMLObjectElement" ],
            [ "HTMLOptionElement" ],
            [ "HTMLScriptElement" ],
            [ "HTMLSelectElement" ],
            [ "HTMLStyleElement" ],
            [ "HTMLTableCellElement" ],
            [ "HTMLTableDataCellElement" ],
            [ "HTMLTableElement" ],
            [ "HTMLTableRowElement" ],
            [ "HTMLTextAreaElement" ],
            [ "HTMLTitleElement" ],
            [ "HTMLUnknownElement" ],
            [ "KeyboardEvent" ],
            [ "MediaQueryList", 10 ],
            [ "MessageEvent" ],
            [ "MimeTypeArray", 11 ],
            [ "MouseEvent" ],
            [ "MSCSSProperties" ],
            [ "MSCSSRuleList" ],
            [ "MSCurrentStyleCSSProperties" ],
            [ "MSEventObj" ],
            [ "MSNamespaceInfoCollection", 0, 9 ],
            [ "MSSelection", 0, 10 ],
            [ "MSStyleCSSProperties" ],
            [ "MutationObserver", 11 ],
            [ "NamedNodeMap" ],
            [ "Navigator" ],
            [ "Node" ],
            [ "NodeList" ],
            [ "PageTransitionEvent", 11 ],
            [ "Performance" ],
            [ "PerformanceNavigation" ],
            [ "PerformanceTiming" ],
            [ "PluginArray", 11 ],
            [ "ProgressEvent", 10 ],
            [ "Range" ],
            [ "Screen" ],
            [ "Storage" ],
            [ "StorageEvent" ],
            [ "StyleSheet" ],
            [ "StyleSheetList" ],
            [ "Text" ],
            [ "TextRange" ],
            [ "UIEvent" ],
            [ "Window" ],
            [ "XMLHttpRequest" ]
        ];
        for(var i = 0; i < ctors.length; i++) {
            if((ctors[i].length > 1 && v < ctors[i][1]) || (ctors[i].length > 2 && v > ctors[i][2]))
                ok(!(ctors[i][0] in window), ctors[i][0] + " in window.");
            else
                ok(Object.prototype.hasOwnProperty.call(window, ctors[i][0]), ctors[i][0] + " not a property of window.");
        }
    }
});

sync_test("elem_props", function() {
    var elem = document.documentElement;

    function test_exposed(prop, expect, is_todo) {
        var ok_ = is_todo ? todo_wine.ok : ok;
        if(expect)
            ok_(prop in elem, prop + " not found in element.");
        else
            ok_(!(prop in elem), prop + " found in element.");
    }

    var v = document.documentMode;

    test_exposed("attachEvent", v < 11);
    test_exposed("detachEvent", v < 11);
    test_exposed("doScroll", v < 11);
    test_exposed("readyState", v < 11);
    test_exposed("clientTop", true);
    test_exposed("title", true);
    test_exposed("querySelectorAll", v >= 8);
    test_exposed("textContent", v >= 9);
    test_exposed("prefix", v >= 9);
    test_exposed("firstElementChild", v >= 9);
    test_exposed("onsubmit", v >= 9);
    test_exposed("getElementsByClassName", v >= 9);
    test_exposed("removeAttributeNS", v >= 9);
    test_exposed("addEventListener", v >= 9);
    test_exposed("hasAttribute", v >= 8, v === 8);
    test_exposed("removeEventListener", v >= 9);
    test_exposed("dispatchEvent", v >= 9);
    test_exposed("msSetPointerCapture", v >= 10);
    if (v >= 9) test_exposed("spellcheck", v >= 10);

    elem = document.createElement("style");
    test_exposed("media", true);
    test_exposed("type", true);
    test_exposed("disabled", true);
    test_exposed("media", true);
    test_exposed("sheet", v >= 9);
    test_exposed("readyState", v < 11);
    test_exposed("styleSheet", v < 11);
    test_exposed("classList", v >= 10);

    elem = document.createElement("img");
    test_exposed("fileSize", v < 11);
});

sync_test("attr_props", function() {
    var elem = document.createElement("style"), attr;
    var v = document.documentMode;
    elem.setAttribute("id", "test");
    elem.setAttribute("test", "wine");
    elem.setAttribute("z-index", "foobar");

    function test_exposed(prop, expect) {
        if(expect)
            ok(prop in attr, prop + " not found in attribute.");
        else
            ok(!(prop in attr), prop + " found in attribute.");
    }

    function test_attr(expando, specified) {
        var r = attr.expando;
        ok(r === expando, attr.name + " attr.expando = " + r);
        r = attr.specified;
        ok(r === specified, attr.name + " attr.specified = " + r);
    }

    attr = elem.getAttributeNode("id");
    test_exposed("appendChild", true);
    test_exposed("attributes", true);
    test_exposed("childNodes", true);
    test_exposed("cloneNode", true);
    test_exposed("compareDocumentPosition", v >= 9);
    test_exposed("expando", true);
    test_exposed("firstChild", true);
    test_exposed("hasChildNodes", true);
    test_exposed("insertBefore", true);
    test_exposed("isDefaultNamespace", v >= 9);
    test_exposed("isEqualNode", v >= 9);
    test_exposed("isSameNode", v >= 9);
    test_exposed("isSupported", v >= 9);
    test_exposed("lastChild", true);
    test_exposed("localName", v >= 9);
    test_exposed("lookupNamespaceURI", v >= 9);
    test_exposed("lookupPrefix", v >= 9);
    test_exposed("name", true);
    test_exposed("namespaceURI", v >= 9);
    test_exposed("nextSibling", true);
    test_exposed("nodeName", true);
    test_exposed("nodeType", true);
    test_exposed("nodeValue", true);
    test_exposed("ownerDocument", true);
    test_exposed("parentNode", true);
    test_exposed("prefix", v >= 9);
    test_exposed("previousSibling", true);
    test_exposed("removeChild", true);
    test_exposed("replaceChild", true);
    test_exposed("specified", true);
    test_exposed("textContent", v >= 9);
    test_exposed("value", true);
    test_attr(false, true);

    attr = elem.getAttributeNode("test");
    test_attr(true, true);

    attr = elem.getAttributeNode("z-index");
    test_attr(true, true);

    attr = elem.getAttributeNode("tabIndex");
    if(v < 8)
        test_attr(false, false);
    else
        todo_wine_if(v === 8).
        ok(attr === null, "tabIndex attr not null.");
});

sync_test("doc_props", function() {
    function test_exposed(prop, expect, is_todo) {
        var ok_ = is_todo ? todo_wine.ok : ok;
        if(expect)
            ok_(prop in document, prop + " not found in document.");
        else
            ok_(!(prop in document), prop + " found in document.");
    }

    var v = document.documentMode;
    ok(document.mimeType === external.getExpectedMimeType("text/html"), "mimeType = " + document.mimeType);

    test_exposed("attachEvent", v < 11);
    test_exposed("detachEvent", v < 11);
    test_exposed("createStyleSheet",v < 11);
    test_exposed("fileSize", v < 11);
    test_exposed("selection", v < 11);
    test_exposed("onstorage", v < 9);
    test_exposed("textContent", v >= 9);
    test_exposed("prefix", v >= 9);
    test_exposed("defaultView", v >= 9);
    test_exposed("head", v >= 9);
    test_exposed("addEventListener", v >= 9);
    test_exposed("removeEventListener", v >= 9);
    test_exposed("dispatchEvent", v >= 9);
    test_exposed("createEvent", v >= 9);

    test_exposed("parentWindow", true);
    if(v >= 9) ok(document.defaultView === document.parentWindow, "defaultView != parentWindow");
});

sync_test("docfrag_props", function() {
    var docfrag = document.createDocumentFragment();

    function test_exposed(prop, expect) {
        if(expect)
            ok(prop in docfrag, prop + " not found in document fragent.");
        else
            ok(!(prop in docfrag), prop + " found in document fragent.");
    }

    var v = document.documentMode;

    test_exposed("compareDocumentPosition", v >= 9);
});

sync_test("window_props", function() {
    function test_exposed(prop, expect, is_todo) {
        var ok_ = is_todo ? todo_wine.ok : ok;
        if(expect)
            ok_(prop in window, prop + " not found in window.");
        else
            ok_(!(prop in window), prop + " found in window.");
    }

    var v = document.documentMode;

    test_exposed("attachEvent", v < 11);
    test_exposed("detachEvent", v < 11);
    test_exposed("execScript", v < 11);
    test_exposed("createPopup", v < 11);
    test_exposed("postMessage", true);
    test_exposed("sessionStorage", true);
    test_exposed("localStorage", true);
    test_exposed("addEventListener", v >= 9);
    test_exposed("removeEventListener", v >= 9);
    test_exposed("dispatchEvent", v >= 9);
    test_exposed("getSelection", v >= 9);
    test_exposed("onfocusout", v >= 9);
    test_exposed("getComputedStyle", v >= 9);
    test_exposed("cancelAnimationFrame", v >= 10);
    test_exposed("requestAnimationFrame", v >= 10);
    test_exposed("Map", v >= 11);
    test_exposed("Set", v >= 11);
    test_exposed("WeakMap", v >= 11);
    test_exposed("WeakSet", false);
    test_exposed("performance", true);
    test_exposed("console", v >= 10);
    test_exposed("matchMedia", v >= 10);
    test_exposed("msCrypto", v >= 11);
    test_exposed("MutationObserver", v >= 11);
});

sync_test("domimpl_props", function() {
    var domimpl = document.implementation;
    function test_exposed(prop, expect) {
        if(expect)
            ok(prop in domimpl, prop + " not found in DOMImplementation.");
        else
            ok(!(prop in domimpl), prop + " found in DOMImplementation.");
    }

    var v = document.documentMode;

    test_exposed("hasFeature", true);
    test_exposed("createDocument", v >= 9);
    test_exposed("createDocumentType", v >= 9);
    test_exposed("createHTMLDocument", v >= 9);
});

sync_test("xhr_props", function() {
    var xhr = new XMLHttpRequest();

    function test_exposed(prop, expect) {
        if(expect)
            ok(prop in xhr, prop + " not found in XMLHttpRequest.");
        else
            ok(!(prop in xhr), prop + " found in XMLHttpRequest.");
    }

    var v = document.documentMode;

    test_exposed("addEventListener", v >= 9);
    test_exposed("removeEventListener", v >= 9);
    test_exposed("dispatchEvent", v >= 9);
    test_exposed("onabort", v >= 10);
    test_exposed("onerror", v >= 10);
    test_exposed("onloadend", v >= 10);
    test_exposed("onloadstart", v >= 10);
    test_exposed("onprogress", v >= 10);
    test_exposed("ontimeout", true);
    test_exposed("overrideMimeType", v >= 11);
    test_exposed("response", v >= 10);
    test_exposed("responseType", v >= 10);
    test_exposed("timeout", true);
    test_exposed("upload", v >= 10);
    test_exposed("withCredentials", v >= 10);
});

sync_test("stylesheet_props", function() {
    var v = document.documentMode;
    var elem = document.createElement("style");
    document.body.appendChild(elem);
    var sheet = v >= 9 ? elem.sheet : elem.styleSheet;

    function test_exposed(prop, expect) {
        if(expect)
            ok(prop in sheet, prop + " not found in style sheet.");
        else
            ok(!(prop in sheet), prop + " found in style sheet.");
    }

    test_exposed("href", true);
    test_exposed("title", true);
    test_exposed("type", true);
    test_exposed("media", true);
    test_exposed("ownerNode", v >= 9);
    test_exposed("ownerRule", v >= 9);
    test_exposed("cssRules", v >= 9);
    test_exposed("insertRule", v >= 9);
    test_exposed("deleteRule", v >= 9);
    test_exposed("disabled", true);
    test_exposed("parentStyleSheet", true);
    test_exposed("owningElement", true);
    test_exposed("readOnly", true);
    test_exposed("imports", true);
    test_exposed("id", true);
    test_exposed("addImport", true);
    test_exposed("addRule", true);
    test_exposed("removeImport", true);
    test_exposed("removeRule", true);
    test_exposed("cssText", true);
    test_exposed("rules", true);
});

sync_test("rect_props", function() {
    document.body.innerHTML = '<div>test</div>';
    var elem = document.body.firstChild;
    var rect = elem.getBoundingClientRect();
    function test_exposed(prop, expect) {
        if(expect)
            ok(prop in rect, prop + " not found in rect object.");
        else
            ok(!(prop in rect), prop + " found in rect object.");
    }

    var v = document.documentMode;

    test_exposed("width", v >= 9);
    test_exposed("height", v >= 9);
});

sync_test("xhr open", function() {
    var e = false;
    try {
        (new XMLHttpRequest()).open("GET", "https://www.winehq.org/");
    }catch(ex) {
        e = true;
    }

    if(document.documentMode < 10)
        ok(e, "expected exception");
    else
        ok(!e, "unexpected exception");
});

sync_test("style_props", function() {
    var style = document.body.style;

    function test_exposed(prop, expect) {
        if(expect)
            ok(prop in style, prop + " not found in style object.");
        else
            ok(!(prop in style), prop + " found in style object.");
    }

    var v = document.documentMode;

    test_exposed("removeAttribute", true);
    test_exposed("zIndex", true);
    test_exposed("z-index", true);
    test_exposed("filter", true);
    test_exposed("pixelTop", true);
    test_exposed("float", true);
    test_exposed("css-float", false);
    test_exposed("style-float", false);
    test_exposed("setProperty", v >= 9);
    test_exposed("removeProperty", v >= 9);
    test_exposed("background-clip", v >= 9);
    test_exposed("msTransform", v >= 9);
    test_exposed("transform", v >= 10);

    style = document.body.currentStyle;

    test_exposed("zIndex", true);
    test_exposed("z-index", true);
    test_exposed("filter", true);
    test_exposed("pixelTop", false);
    test_exposed("float", true);
    test_exposed("css-float", false);
    test_exposed("style-float", false);
    test_exposed("styleFloat", true);
    test_exposed("setProperty", v >= 9);
    test_exposed("removeProperty", v >= 9);
    test_exposed("background-clip", v >= 9);
    test_exposed("transform", v >= 10);
    test_exposed("zoom", true);

    try {
        style.styleFloat = "left";
        ok(false, "expected exception setting styleFloat");
    }catch(ex) {}
    try {
        style.zoom = "1.0";
        ok(false, "expected exception setting zoom");
    }catch(ex) {}

    if(window.getComputedStyle) {
        style = window.getComputedStyle(document.body);

        test_exposed("removeAttribute", false);
        test_exposed("zIndex", true);
        test_exposed("z-index", true);
        test_exposed("pixelTop", false);
        test_exposed("float", true);
        test_exposed("css-float", false);
        test_exposed("style-float", false);
        test_exposed("setProperty", v >= 9);
        test_exposed("removeProperty", v >= 9);
        test_exposed("background-clip", v >= 9);
        test_exposed("transform", v >= 10);
    }
});

sync_test("createElement_inline_attr", function() {
    var v = document.documentMode, e, s;

    if(v < 9) {
        s = document.createElement("<div>").tagName;
        ok(s === "DIV", "<div>.tagName returned " + s);
        s = document.createElement("<div >").tagName;
        ok(s === "DIV", "<div >.tagName returned " + s);
        s = document.createElement("<div/>").tagName;
        ok(s === "DIV", "<div/>.tagName returned " + s);
        e = 0;
        try {
            document.createElement("<div");
        }catch(ex) {
            e = ex.number;
        }
        ok(e === 0x4005 - 0x80000000, "<div e = " + e);
        e = 0;
        try {
            document.createElement("<div test=1");
        }catch(ex) {
            e = ex.number;
        }
        ok(e === 0x4005 - 0x80000000, "<div test=1 e = " + e);

        var tags = [ "div", "head", "body", "title", "html" ];

        for(var i = 0; i < tags.length; i++) {
            e = document.createElement("<" + tags[i] + " test='a\"' abcd=\"&quot;b&#34;\">");
            ok(e.tagName === tags[i].toUpperCase(), "<" + tags[i] + " test=\"a\" abcd=\"b\">.tagName returned " + e.tagName);
            ok(e.test === "a\"", "<" + tags[i] + " test='a\"' abcd=\"&quot;b&#34;\">.test returned " + e.test);
            ok(e.abcd === "\"b\"", "<" + tags[i] + " test='a\"' abcd=\"&quot;b&#34;\">.abcd returned " + e.abcd);
        }
    }else {
        s = "";
        e = 0;
        try {
            document.createElement("<div>");
        }catch(ex) {
            s = ex.toString();
            e = ex.number;
        }
        todo_wine.
        ok(e === undefined, "<div> e = " + e);
        todo_wine.
        ok(s === "InvalidCharacterError", "<div> s = " + s);
        s = "";
        e = 0;
        try {
            document.createElement("<div test=\"a\">");
        }catch(ex) {
            s = ex.toString();
            e = ex.number;
        }
        todo_wine.
        ok(e === undefined, "<div test=\"a\"> e = " + e);
        todo_wine.
        ok(s === "InvalidCharacterError", "<div test=\"a\"> s = " + s);
    }
});

sync_test("JS objs", function() {
    var g = window;

    function test_exposed(func, obj, expect, is_todo) {
        var ok_ = is_todo ? todo_wine.ok : ok;
        if(expect)
            ok_(func in obj, func + " not found in " + obj);
        else
            ok_(!(func in obj), func + " found in " + obj);
    }

    function test_parses(code, expect) {
        var success;
        try {
            eval(code);
            success = true;
        }catch(e) {
            success = false;
        }
        if(expect)
            ok(success === true, code + " did not parse");
        else
            ok(success === false, code + " parsed");
    }

    var v = document.documentMode;

    test_exposed("ScriptEngineMajorVersion", g, true);

    test_exposed("JSON", g, v >= 8);
    test_exposed("now", Date, true);
    test_exposed("toISOString", Date.prototype, v >= 9);
    test_exposed("isArray", Array, v >= 9);
    test_exposed("forEach", Array.prototype, v >= 9);
    test_exposed("indexOf", Array.prototype, v >= 9);
    test_exposed("trim", String.prototype, v >= 9);
    test_exposed("map", Array.prototype, v >= 9);

    /* FIXME: IE8 implements weird semi-functional property descriptors. */
    test_exposed("getOwnPropertyDescriptor", Object, v >= 8, v === 8);
    test_exposed("defineProperty", Object, v >= 8, v === 8);
    test_exposed("defineProperties", Object, v >= 9);

    test_exposed("getPrototypeOf", Object, v >= 9);

    test_parses("if(false) { o.default; }", v >= 9);
    test_parses("if(false) { o.with; }", v >= 9);
    test_parses("if(false) { o.if; }", v >= 9);
});

sync_test("eval", function() {
    var i, context, code = "this.foobar = 1234", v = document.documentMode;

    var direct = [
        function() { eval(code); },
        function() { (eval)(code); },
        function() { (function(eval) { eval(code); }).call(this, eval); },
        function() { eval("eval(" + code + ")"); }
    ];

    for(i = 0; i < direct.length; i++) {
        context = {};
        direct[i].call(context);
        ok(context.foobar === 1234, "direct[" + i + "] context foobar = " + context.foobar);
    }

    var indirect = [
        function() { (true, eval)(code); },
        function() { (eval, eval)(code); },
        function() { (true ? eval : false)(code); },
        function() { [eval][0](code); },
        function() { eval.call(this, code); },
        function() { var f; (f = eval)(code); },
        function() { var f = eval; f(code); },
        function() { (function(f) { f(code); }).call(this, eval); },
        function() { (function(f) { return f; }).call(this, eval)(code); },
        function() { (function() { arguments[0](code) }).call(this, eval); },
        function() { window.eval(code); },
        function() { window["eval"](code); },
        function() { eval("eval")(code); }
    ];

    for(i = 0; i < indirect.length; i++) {
        context = {};
        ok(!("foobar" in window), "indirect[" + i + "] has global foobar before call");
        indirect[i].call(context);
        if(v < 9) {
            ok(context.foobar === 1234, "indirect[" + i + "] context foobar = " + context.foobar);
            ok(!("foobar" in window), "indirect[" + i + "] has global foobar");
        }else {
            ok(!("foobar" in context), "indirect[" + i + "] has foobar");
            ok(window.foobar === 1234, "indirect[" + i + "] global foobar = " + context.foobar);
            delete window.foobar;
        }
    }

    context = {};
    (function(eval) { eval(code); })(function() { context.barfoo = 4321; });
    ok(context.barfoo === 4321, "context.barfoo = " + context.barfoo);
});

sync_test("for..in", function() {
    var v = document.documentMode, found = 0, r;

    function ctor() {}
    ctor.prototype.test2 = true;

    var arr = new Array(), obj = new ctor(), i, r;
    obj.test1 = true;

    i = 0;
    for(r in obj) {
        ctor.prototype.test3 = true;
        arr[r] = true;
        i++;
    }

    ok(i === 3, "enum did " + i + " iterations");
    ok(arr["test1"] === true, "arr[test1] !== true");
    ok(arr["test2"] === true, "arr[test2] !== true");
    ok(arr["test3"] === true, "arr[test3] !== true");

    for(r in document)
        if(r === "ondragstart")
            found++;
    ok(found === 1, "ondragstart enumerated " + found + " times in document");
    document.ondragstart = "";
    found = 0;
    for(r in document)
        if(r === "ondragstart")
            found++;
    ok(found === 1, "ondragstart enumerated " + found + " times in document after set to empty string");
});

sync_test("function caller", function() {
    ok(Function.prototype.hasOwnProperty("caller"), "caller not prop of Function.prototype");

    function test_caller(expected_caller, stop) {
        ok(test_caller.caller === expected_caller, "caller = " + test_caller.caller);
        if(stop) return;
        function nested() {
            ok(nested.caller === test_caller, "nested caller = " + nested.caller);
            test_caller(nested, true);
            ok(test_caller.caller === expected_caller, "caller within nested = " + test_caller.caller);
        }
        nested();
        ok(test_caller.caller === expected_caller, "caller after nested = " + test_caller.caller);
    }
    ok(test_caller.hasOwnProperty("caller"), "caller not prop of test_caller");
    ok(test_caller.caller === null, "test_caller.caller = " + test_caller.caller);

    function f1() { test_caller(f1); } f1();
    function f2() { test_caller(f2); } f2();
});

sync_test("elem_by_id", function() {
    document.body.innerHTML = '<form id="testid" name="testname"></form>';
    var v = document.documentMode, found, i;

    var id_elem = document.getElementById("testid");
    ok(id_elem.tagName === "FORM", "id_elem.tagName = " + id_elem.tagName);

    var name_elem = document.getElementById("testname");
    if(v < 8)
        ok(id_elem === name_elem, "id_elem != id_elem");
    else
        ok(name_elem === null, "name_elem != null");

    id_elem = window.testid;
    ok(id_elem.tagName === "FORM", "window.testid = " + id_elem);

    name_elem = document.testname;
    ok(name_elem.tagName === "FORM", "document.testname = " + name_elem);

    for(id_elem in window)
        ok(id_elem !== "testid" && id_elem != "testname", id_elem + " was enumerated in window");
    window.testid = 137;
    found = false;
    for(id_elem in window) {
        ok(id_elem != "testname", id_elem + " was enumerated in window after set to 137");
        if(id_elem === "testid")
            found = true;
    }
    ok(found, "testid was not enumerated in window after set to 137");

    found = false;
    for(id_elem in document) {
        ok(id_elem !== "testid", "testid was enumerated in document");
        if(id_elem === "testname")
            found = true;
    }
    ok(found, "testname was not enumerated in document");

    try {
        document.testname();
        ok(false, "document.testname() did not throw exception");
    }catch(e) {
        ok(e.number === 0xa01b6 - 0x80000000, "document.testname() threw = " + e.number);
    }

    try {
        document.testname = "foo";
        ok(v >= 9, "Setting document.testname did not throw exception");

        id_elem = document.testid;
        ok(id_elem.tagName === "FORM", "document.testid after set = " + id_elem);
        name_elem = document.testname;
        ok(name_elem === "foo", "document.testname after set = " + name_elem);
    }catch(e) {
        ok(v < 9 && e.number === 0xa01b6 - 0x80000000, "Setting document.testname threw = " + e.number);
    }

    try {
        document.testid = "bar";
        ok(v >= 9, "Setting document.testid did not throw exception");

        id_elem = document.testid;
        ok(id_elem === "bar", "document.testid after both set = " + id_elem);
        name_elem = document.testname;
        ok(name_elem === "foo", "document.testname after both set = " + name_elem);

        found = false, name_elem = false;
        for(id_elem in document) {
            if(id_elem === "testid")
                found = true;
            if(id_elem === "testname")
                name_elem = true;
        }
        ok(found, "testid was not enumerated in document after both set");
        ok(name_elem, "testname was not enumerated in document after both set");
        delete document.testid;
        delete document.testname;
    }catch(e) {
        ok(v < 9 && e.number === 0xa01b6 - 0x80000000, "Setting document.testid threw = " + e.number);
    }

    // these tags expose name as props, and id only if they have a name
    var tags = [ "embed", "form", "iframe", "img" ];
    for(i in tags) {
        var tag = tags[i];
        document.body.innerHTML = '<' + tag + ' id="testid" name="testname"></' + tag + '><' + tag + ' id="foobar"></' + tag + '>';
        ok("testname" in document, tag + " did not expose testname");
        ok("testid" in document, tag + " did not expose testid");
        ok(!("foobar" in document), tag + " exposed foobar");
    }

    // these tags always expose their id as well as name (we don't test applet because it makes Windows pop up a dialog box)
    tags = [ "object" ];
    for(i in tags) {
        var tag = tags[i];
        document.body.innerHTML = '<' + tag + ' id="testid" name="testname"></' + tag + '><' + tag + ' id="foobar"></' + tag + '>';
        ok("testname" in document, tag + " did not expose testname");
        ok("testid" in document, tag + " did not expose testid");
        ok("foobar" in document, tag + " did not expose foobar");
    }

    // all other tags don't expose props for either id or name, test a few of them here
    tags = [ "a", "b", "body", "center", "div", "frame", "h2", "head", "html", "input", "meta", "p", "span", "style", "table", "winetest" ];
    for(i in tags) {
        var tag = tags[i];
        document.body.innerHTML = '<' + tag + ' id="testid" name="testname"></' + tag + '><' + tag + ' id="foobar"></' + tag + '>';
        ok(!("testname" in document), tag + " exposed testname");
        ok(!("testid" in document), tag + " exposed testid");
        ok(!("foobar" in document), tag + " exposed foobar");
    }
});

sync_test("doc_mode", function() {
    compat_version = parseInt(document.location.search.substring(1));

    trace("Testing compatibility mode " + compat_version);

    if(compat_version > 6 && compat_version > document.documentMode) {
        win_skip("Document mode not supported (expected " + compat_version + " got " + document.documentMode + ")");
        reportSuccess();
        return;
    }

    ok(Math.max(compat_version, 5) === document.documentMode, "documentMode = " + document.documentMode);

    if(document.documentMode > 5)
        ok(document.compatMode === "CSS1Compat", "document.compatMode = " + document.compatMode);
    else
        ok(document.compatMode === "BackCompat", "document.compatMode = " + document.compatMode);
});

sync_test("doctype", function() {
    var doctype = document.doctype;

    if(document.documentMode < 9) {
        ok(doctype === null, "doctype = " + document.doctype);
        return;
    }

    ok(doctype.name === "html", "doctype.name = " + doctype.name);
});

async_test("iframe_doc_mode", function() {
    var iframe = document.createElement("iframe");

    iframe.onload = function() {
        var iframe_mode = iframe.contentWindow.document.documentMode;
        if(document.documentMode < 9)
            ok(iframe_mode === 5, "iframe_mode = " + iframe_mode);
        else
            ok(iframe_mode === document.documentMode, "iframe_mode = " + iframe_mode);
        next_test();
    }

    iframe.src = "about:blank";
    document.body.appendChild(iframe);
});

sync_test("conditional_comments", function() {
    var div = document.createElement("div");
    document.body.appendChild(div);

    function test_version(v) {
        var version = compat_version ? compat_version : 7;

        div.innerHTML = "<!--[if lte IE " + v + "]>true<![endif]-->";
        ok(div.innerText === (version <= v ? "true" : ""),
           "div.innerText = " + div.innerText + " for version (<=) " + v);

        div.innerHTML = "<!--[if lt IE " + v + "]>true<![endif]-->";
        ok(div.innerText === (version < v ? "true" : ""),
           "div.innerText = " + div.innerText + " for version (<) " + v);

        div.innerHTML = "<!--[if gte IE " + v + "]>true<![endif]-->";
        ok(div.innerText === (version >= v && version < 10 ? "true" : ""),
           "div.innerText = " + div.innerText + " for version (>=) " + v);

        div.innerHTML = "<!--[if gt IE " + v + "]>true<![endif]-->";
        ok(div.innerText === (version > v && version < 10 ? "true" : ""),
           "div.innerText = " + div.innerText + " for version (>) " + v);
    }

    test_version(5);
    test_version(6);
    test_version(7);
    test_version(8);
});

var ready_states;

async_test("script_load", function() {
    var v = document.documentMode;
    if(v < 9) {
        next_test();
        return;
    }

    var elem = document.createElement("script");
    ready_states = "";

    elem.onreadystatechange = guard(function() {
        ok(v < 11, "unexpected onreadystatechange call");
        ready_states += elem.readyState + ",";
    });

    elem.onload = guard(function() {
        switch(v) {
        case 9:
            ok(ready_states === "loading,exec,loaded,", "ready_states = " + ready_states);
            break;
        case 10:
            ok(ready_states === "loading,exec,", "ready_states = " + ready_states);
            break;
        case 11:
            ok(ready_states === "exec,", "ready_states = " + ready_states);
            break;
        }
        next_test();
    });

    document.body.appendChild(elem);
    elem.src = "jsstream.php?simple";
    external.writeStream("simple", "ready_states += 'exec,';");
});

sync_test("location", function() {
    document.body.innerHTML = '<a name="testanchor">test</a>';

    ok(location.hash === "", "initial location.hash = " + location.hash);
    location.hash = "TestAnchor";
    ok(location.hash === "#TestAnchor", "location.hash after set to TestAnchor = " + location.hash);
    location.hash = "##foo";
    ok(location.hash === "##foo", "location.hash after set to ##foo = " + location.hash);
    location.hash = "#testanchor";
    ok(location.hash === "#testanchor", "location.hash after set to #testanchor = " + location.hash);
});

sync_test("navigator", function() {
    var v = document.documentMode, re;
    var app = navigator.appVersion;
    ok(navigator.userAgent === "Mozilla/" + app,
       "userAgent = " + navigator.userAgent + " appVersion = " + app);

    re = v < 11
        ? "^" + (v < 9 ? "4" : "5") + "\\.0 \\(compatible; MSIE " + (v < 7 ? 7 : v) +
          "\\.0; Windows NT [0-9].[0-9]; .*Trident/[678]\\.0.*\\)$"
        : "^5.0 \\(Windows NT [0-9].[0-9]; .*Trident/[678]\\.0.*rv:11.0\\) like Gecko$";
    ok(new RegExp(re).test(app), "appVersion = " + app);

    ok(navigator.appCodeName === "Mozilla", "appCodeName = " + navigator.appCodeName);
    ok(navigator.appName === (v < 11 ? "Microsoft Internet Explorer" : "Netscape"),
       "appName = " + navigator.appName);
    ok(navigator.toString() === (v < 9 ? "[object]" : "[object Navigator]"),
       "navigator.toString() = " + navigator.toString());
});

sync_test("delete_prop", function() {
    var v = document.documentMode;
    var obj = document.createElement("div"), r, obj2, func, prop;

    obj.prop1 = true;
    r = false;
    try {
        delete obj.prop1;
    }catch(ex) {
        r = true;
    }
    if(v < 8) {
        ok(r, "did not get an expected exception");
        return;
    }
    ok(!r, "got an unexpected exception");
    ok(!("prop1" in obj), "prop1 is still in obj");

    /* builtin properties don't throw any exception, but are not really deleted */
    r = (delete obj.tagName);
    ok(r, "delete returned " + r);
    ok("tagName" in obj, "tagName deleted from obj");
    ok(obj.tagName === "DIV", "tagName = " + obj.tagName);

    prop = obj.id;
    r = (delete obj.id);
    ok(r, "delete returned " + r);
    ok("id" in obj, "id deleted from obj");
    ok(obj.id === prop, "id = " + obj.id);

    obj.id = "1234";
    ok(obj.id === "1234", "id after set to 1234 = " + obj.id);
    r = (delete obj.id);
    ok(r, "delete returned " + r);
    ok("id" in obj, "id deleted from obj");
    ok(obj.id === "1234", "id = " + obj.id);

    /* builtin functions get reset to their original values */
    func = function() { }
    prop = obj.setAttribute;
    r = (delete obj.setAttribute);
    ok(r, "delete returned " + r);
    ok("setAttribute" in obj, "setAttribute deleted from obj");
    ok(obj.setAttribute === prop, "setAttribute = " + obj.setAttribute);

    obj.setAttribute = func;
    ok(obj.setAttribute === func, "setAttribute after set to func = " + obj.setAttribute);
    r = (delete obj.setAttribute);
    ok(r, "delete returned " + r);
    ok("setAttribute" in obj, "setAttribute deleted from obj");
    ok(obj.setAttribute === prop, "setAttribute = " + obj.setAttribute);

    /* again, this time prop1 does not exist */
    r = false;
    try {
        delete obj.prop1;
    }catch(ex) {
        r = true;
    }
    if(v < 9) {
        ok(r, "did not get an expected exception");
        return;
    }else {
        ok(!r, "got an unexpected exception");
        ok(!("prop1" in obj), "prop1 is still in obj");
    }

    r = (delete obj.className);
    ok(r, "delete returned " + r);
    ok("className" in obj, "className deleted from obj");
    ok(obj.className === "", "className = " + obj.className);

    obj = document.querySelectorAll("*");
    ok("0" in obj, "0 is not in obj");
    obj2 = obj[0];
    r = (delete obj[0]);
    ok("0" in obj, "0 is not in obj");
    ok(obj[0] === obj2, "obj[0] != obj2");

    /* test window object and its global scope handling */
    obj = window;

    obj.globalprop1 = true;
    ok(globalprop1, "globalprop1 = " + globalprop1);
    r = false;
    try {
        delete obj.globalprop1;
    }catch(ex) {
        r = true;
    }
    if(v < 9) {
        ok(r, "did not get an expected exception");
    }else {
        ok(!r, "got an unexpected globalprop1 exception");
        ok(!("globalprop1" in obj), "globalprop1 is still in obj");
    }

    globalprop2 = true;
    ok(obj.globalprop2, "globalprop2 = " + globalprop2);
    r = false;
    try {
        delete obj.globalprop2;
    }catch(ex) {
        r = true;
    }
    if(v < 9) {
        ok(r, "did not get an expected globalprop2 exception");
    }else {
        ok(!r, "got an unexpected exception");
        ok(!("globalprop2" in obj), "globalprop2 is still in obj");
    }

    obj.globalprop3 = true;
    ok(globalprop3, "globalprop3 = " + globalprop3);
    r = false;
    try {
        delete globalprop3;
    }catch(ex) {
        r = true;
    }
    if(v < 9) {
        ok(r, "did not get an expected exception");
        ok("globalprop3" in obj, "globalprop3 is not in obj");
    }else {
        ok(!r, "got an unexpected globalprop3 exception");
        ok(!("globalprop3" in obj), "globalprop3 is still in obj");
    }

    globalprop4 = true;
    ok(obj.globalprop4, "globalprop4 = " + globalprop4);
    r = (delete globalprop4);
    ok(r, "delete returned " + r);
    ok(!("globalprop4" in obj), "globalprop4 is still in obj");
});

sync_test("detached arguments", function() {
    var args, get_a, set_a, get_x, set_x;

    function test_args() {
        ok(args[0] === 1, "args[0] = " + args[0]);
        set_x(2);
        ok(args[0] === 2, "args[0] = " + args[0]);
        args[0] = 3;
        ok(get_x() === 3, "get_x() = " + get_x());
        ok(args[0] === 3, "args[0] = " + args[0]);
    }

    (function(x) {
        args = arguments;
        get_x = function() { return x; };
        set_x = function(v) { x = v; };

        test_args();
        x = 1;
    })(1);
    test_args();

    (function(a, a, b, c) {
        get_a = function() { return a; }
        set_a = function(v) { a = v; }
        ok(get_a() === 2, "get_a() = " + get_a());
        ok(a === 2, "a = " + a);
        ok(b === 3, "b = " + b);
        ok(c === 4, "c = " + c);
        a = 42;
        ok(arguments[0] === 1, "arguments[0] = " + arguments[0]);
        ok(arguments[1] === 42, "arguments[1] = " + arguments[1]);
        ok(get_a() === 42, "get_a() after assign = " + get_a());
        args = arguments;
    })(1, 2, 3, 4);

    ok(get_a() === 42, "get_a() after detach = " + get_a());
    set_a(100);
    ok(get_a() === 100, "get_a() after set_a() = " + get_a());
    ok(args[0] === 1, "detached args[0] = " + args[0]);
    ok(args[1] === 100, "detached args[1] = " + args[1]);

    (function(a, a) {
        eval("var a = 7;");
        ok(a === 7, "function(a, a) a = " + a);
        ok(arguments[0] === 5, "function(a, a) arguments[0] = " + arguments[0]);
        ok(arguments[1] === 7, "function(a, a) arguments[1] = " + arguments[1]);
    })(5, 6);
});

var func_scope_val = 1;
var func_scope_val2 = 2;

sync_test("func_scope", function() {
    var func_scope_val = 2;

    var f = function func_scope_val() {
        return func_scope_val;
    };

    func_scope_val = 3;
    if(document.documentMode < 9) {
        ok(f() === 3, "f() = " + f());
        return;
    }
    ok(f === f(), "f() = " + f());

    f = function func_scope_val(a) {
        func_scope_val = 4;
        return func_scope_val;
    };

    func_scope_val = 3;
    ok(f === f(), "f() = " + f());
    ok(func_scope_val === 3, "func_scope_val = " + func_scope_val);
    ok(window.func_scope_val === 1, "window.func_scope_val = " + window.func_scope_val);

    f = function func_scope_val(a) {
        return (function() { return a ? func_scope_val(false) : func_scope_val; })();
    };

    ok(f === f(true), "f(true) = " + f(true));

    window = 1;
    ok(window === window.self, "window = " + window);

    ! function func_scope_val2() {};
    ok(window.func_scope_val2 === 2, "window.func_scope_val2 = " + window.func_scope_val2);

    var o = {};
    (function(x) {
        ok(x === o, "x = " + x);
        ! function x() {};
        ok(x === o, "x != o");
    })(o);

    (function(x) {
        ok(x === o, "x = " + x);
        1, function x() {};
        ok(x === o, "x != o");
    })(o);

    (function() {
        ! function x() {};
        try {
            x();
            ok(false, "expected exception");
        }catch(e) {}
    })(o);
});

sync_test("set_obj", function() {
    if(!("Set" in window)) return;

    try {
        var s = Set();
        ok(false, "expected exception calling constructor as method");
    }catch(e) {
        ok(e.number === 0xa13fc - 0x80000000, "calling constructor as method threw " + e.number);
    }

    var s = new Set, r;
    ok(Object.getPrototypeOf(s) === Set.prototype, "unexpected Set prototype");

    function test_length(name, len) {
        ok(Set.prototype[name].length === len, "Set.prototype." + name + " = " + Set.prototype[name].length);
        try {
            Set.prototype[name].call({}, 0);
            ok(false, "expected exception calling Set.prototype." + name + "(object)");
        }catch(e) {
            ok(e.number === 0xa13fc - 0x80000000, "Set.prototype." + name + "(object) threw " + e.number);
        }
    }
    test_length("add", 1);
    test_length("clear", 0);
    test_length("delete", 1);
    test_length("forEach", 1);
    test_length("has", 1);
    ok(!("entries" in s), "entries are in Set");
    ok(!("keys" in s), "keys are in Set");
    ok(!("values" in s), "values are in Set");

    r = Object.prototype.toString.call(s);
    ok(r === "[object Object]", "toString returned " + r);

    r = s.has(-0);
    ok(r === false, "has(-0) returned " + r);
    ok(s.size === 0, "size = " + s.size);

    r = s.add(42);
    ok(r === undefined, "add(42) returned " + r);
    r = s.add(42);
    ok(r === undefined, "add(42) returned " + r);
    r = s.add(0);
    ok(r === undefined, "add(0) returned " + r);
    r = s.has(-0);
    ok(r === false, "has(-0) returned " + r);
    r = s.add(-0);
    ok(r === undefined, "add(-0) returned " + r);
    r = s.has(-0);
    ok(r === true, "has(-0) after add returned " + r);
    r = s.add("test");
    ok(r === undefined, "add(test) returned " + r);
    r = s.add(13);
    ok(r === undefined, "add(13) returned " + r);
    r = s.add(s);
    ok(r === undefined, "add(s) returned " + r);

    r = s["delete"]("test"); /* using s.delete() would break parsing in quirks mode */
    ok(r === true, "delete(test) returned " + r);
    r = s["delete"]("test");
    ok(r === false, "delete(test) returned " + r);

    ok(s.size === 5, "size = " + s.size);
    s.size = 100;
    ok(s.size === 5, "size (after set) = " + s.size);

    var a = [];
    r = s.forEach(function(value, key, obj) {
        var t = s["delete"](key);
        ok(t === true, "delete(" + key + ") returned " + r);
        ok(value === key, "value = " + value + ", key = " + key);
        ok(obj === s, "set = " + obj);
        ok(this === a, "this = " + this);
        a.push(value);
    }, a);
    ok(r === undefined, "forEach returned " + r);
    ok(a.length === 5, "a.length = " + a.length);
    for(var i = 0; i < a.length; i++)
        ok(a[i] === [42, 0, -0, 13, s][i], "a[" + i + "] = " + a[i]);
    ok(s.size === 0, "size = " + s.size);

    s = new Set();
    ok(s.size === 0, "size = " + s.size);
    s.add(1);
    s.add(2);
    ok(s.size === 2, "size = " + s.size);
    r = s.clear();
    ok(r === undefined, "clear returned " + r);
    ok(s.size === 0, "size = " + s.size);

    s = new Set([1, 2, 3]);
    ok(s.size === 0, "size = " + s.size);

    s = new Set();
    s.add(1);
    s.add(2);
    s.add(3);
    r = 0;
    s.forEach(function(value, key, obj) {
        r++;
        s.clear();
        ok(s.size === 0, "size = " + s.size);
        ok(this.valueOf() === 42, "this.valueOf() = " + this.valueOf());
    }, 42);
    ok(r === 1, "r = " + r);
});

sync_test("map_obj", function() {
    if(!("Map" in window)) return;

    try {
        var s = Map();
        ok(false, "expected exception calling constructor as method");
    }catch(e) {
        ok(e.number === 0xa13fc - 0x80000000, "calling constructor as method threw " + e.number);
    }

    var s = new Map, r, i;
    ok(Object.getPrototypeOf(s) === Map.prototype, "unexpected Map prototype");

    function test_length(name, len) {
        ok(Map.prototype[name].length === len, "Map.prototype." + name + " = " + Map.prototype[name].length);
    }
    test_length("clear", 0);
    test_length("delete", 1);
    test_length("forEach", 1);
    test_length("get", 1);
    test_length("has", 1);
    test_length("set", 2);
    ok(!("entries" in s), "entries are in Map");
    ok(!("keys" in s), "keys are in Map");
    ok(!("values" in s), "values are in Map");
    todo_wine.
    ok("size" in Map.prototype, "size is not in Map.prototype");

    r = Object.prototype.toString.call(s);
    ok(r === "[object Object]", "toString returned " + r);

    r = s.get("test");
    ok(r === undefined, "get(test) returned " + r);
    r = s.has("test");
    ok(r === false, "has(test) returned " + r);
    ok(s.size === 0, "size = " + s.size + " expected 0");

    r = s.set("test", 1);
    ok(r === undefined, "set returned " + r);
    ok(s.size === 1, "size = " + s.size + " expected 1");
    r = s.get("test");
    ok(r === 1, "get(test) returned " + r);
    r = s.has("test");
    ok(r === true, "has(test) returned " + r);

    s.size = 100;
    ok(s.size === 1, "size = " + s.size + " expected 1");

    s.set("test", 2);
    r = s.get("test");
    ok(r === 2, "get(test) returned " + r);
    r = s.has("test");
    ok(r === true, "has(test) returned " + r);

    r = s["delete"]("test"); /* using s.delete() would break parsing in quirks mode */
    ok(r === true, "delete(test) returned " + r);
    ok(s.size === 0, "size = " + s.size + " expected 0");
    r = s["delete"]("test");
    ok(r === false, "delete(test) returned " + r);

    var test_keys = [undefined, null, NaN, 3, "str", false, true, {}];
    for(i in test_keys) {
        r = s.set(test_keys[i], test_keys[i] + 1);
        ok(r === undefined, "set(test) returned " + r);
    }
    ok(s.size === test_keys.length, "size = " + s.size + " expected " + test_keys.length);
    for(i in test_keys) {
        r = s.get(test_keys[i]);
        if(isNaN(test_keys[i]))
            ok(isNaN(r), "get(" + test_keys[i] + ") returned " + r);
        else
            ok(r === test_keys[i] + 1, "get(" + test_keys[i] + ") returned " + r);
    }

    var calls = [];
    i = 0;
    r = s.forEach(function(value, key, map) {
        if(isNaN(test_keys[i])) {
            ok(isNaN(key), "key = " + key + " expected NaN");
            ok(isNaN(value), "value = " + value + " expected NaN");
        }else {
            ok(key === test_keys[i], "key = " + key + " expected " + test_keys[i]);
            ok(value === key + 1, "value = " + value);
        }
        ok(map === s, "map = " + map);
        ok(this === test_keys, "this = " + this);
        i++;
    }, test_keys);
    ok(i === test_keys.length, "i = " + i);
    ok(r === undefined, "forEach returned " + r);

    s.set(3, "test2")
    calls = [];
    i = 0;
    s.forEach(function(value, key) {
        if(isNaN(test_keys[i]))
            ok(isNaN(key), "key = " + key + " expected " + test_keys[i]);
        else
            ok(key === test_keys[i], "key = " + key + " expected " + test_keys[i]);
        i++;
    });
    ok(i === test_keys.length, "i = " + i);

    r = s.clear();
    ok(r === undefined, "clear returned " + r);
    ok(s.size === 0, "size = " + s.size + " expected 0");
    r = s.get(test_keys[0]);
    ok(r === undefined, "get returned " + r);

    s = new Map();
    s.set(1, 10);
    s.set(2, 20);
    s.set(3, 30);
    i = true;
    s.forEach(function() {
        ok(i, "unexpected call");
        s.clear();
        i = false;
    });

    s = new Map();
    s.set(1, 10);
    s.set(2, 20);
    s.set(3, 30);
    i = 0;
    s.forEach(function(value, key) {
        i += key + value;
        r = s["delete"](key);
        ok(r === true, "delete returned " + r);
    });
    ok(i === 66, "i = " + i);

    s = new Map();
    s.set(0,  10);
    s.set(-0, 20);
    ok(s.size === 2, "size = " + s.size + " expected 2");
    r = s.get(-0);
    ok(r === 20, "get(-0) returned " + r);
    r = s.get(0);
    ok(r === 10, "get(0) returned " + r);

    try {
        Map.prototype.set.call({}, 1, 2);
        ok(false, "expected exception");
    }catch(e) {
        ok(e.number === 0xa13fc - 0x80000000, "e.number = " + e.number);
    }

    s = new Map();
    s.set(1, 10);
    s.set(2, 20);
    s.set(3, 30);
    r = 0;
    s.forEach(function(value, key) {
        r++;
        s.clear();
        ok(s.size === 0, "size = " + s.size);
        ok(this.valueOf() === 42, "this.valueOf() = " + this.valueOf());
    }, 42);
    ok(r === 1, "r = " + r);
});

async_test("weakmap_obj", function() {
    if(!("WeakMap" in window)) { next_test(); return; }

    try {
        var s = WeakMap();
        ok(false, "expected exception calling constructor as method");
    }catch(e) {
        ok(e.number === 0xa13fc - 0x80000000, "calling constructor as method threw " + e.number);
    }

    var s = new WeakMap, r, o, o2;
    ok(Object.getPrototypeOf(s) === WeakMap.prototype, "unexpected WeakMap prototype");

    function test_length(name, len) {
        ok(WeakMap.prototype[name].length === len, "WeakMap.prototype." + name + " = " + WeakMap.prototype[name].length);
    }
    test_length("clear", 0);
    test_length("delete", 1);
    test_length("get", 1);
    test_length("has", 1);
    test_length("set", 2);
    ok(!("entries" in s), "entries is in WeakMap");
    ok(!("forEach" in s), "forEach is in WeakMap");
    ok(!("keys" in s), "keys is in WeakMap");
    ok(!("size" in s), "size is in WeakMap");
    ok(!("values" in s), "values is in WeakMap");

    r = Object.prototype.toString.call(s);
    ok(r === "[object Object]", "toString returned " + r);

    r = s.get("test");
    ok(r === undefined, "get('test') returned " + r);
    r = s.has("test");
    ok(r === false, "has('test') returned " + r);

    try {
        r = s.set("test", 1);
        ok(false, "set('test') did not throw");
    }catch(e) {
        ok(e.number === 0xa13fd - 0x80000000, "set('test') threw " + e.number);
    }
    try {
        r = s.set(external.testHostContext(true), 1);
        ok(false, "set(host_obj) did not throw");
    }catch(e) {
        ok(e.number === 0xa13fd - 0x80000000, "set(host_obj) threw " + e.number);
    }

    r = s.set({}, 1);
    ok(r === undefined, "set({}, 1) returned " + r);

    o = {}, o2 = {};
    r = s.get({});
    ok(r === undefined, "get({}) returned " + r);
    r = s.has({});
    ok(r === false, "has({}) returned " + r);

    r = s.set(o, 2);
    ok(r === undefined, "set(o, 2) returned " + r);
    r = s.get(o);
    ok(r === 2, "get(o) returned " + r);
    r = s.has(o);
    ok(r === true, "has(o) returned " + r);
    r = s.get(o2);
    ok(r === undefined, "get(o2) before set returned " + r);
    r = s.has(o2);
    ok(r === false, "has(o2) before set returned " + r);
    r = s.set(o2, "test");
    ok(r === undefined, "set(o2, 'test') returned " + r);
    r = s.get(o2);
    ok(r === "test", "get(o2) returned " + r);
    r = s.has(o2);
    ok(r === true, "has(o2) returned " + r);

    r = s["delete"]("test"); /* using s.delete() would break parsing in quirks mode */
    ok(r === false, "delete('test') returned " + r);
    r = s["delete"]({});
    ok(r === false, "delete({}) returned " + r);
    r = s["delete"](o);
    ok(r === true, "delete(o) returned " + r);

    r = s.get(o);
    ok(r === undefined, "get(o) after delete returned " + r);
    r = s.has(o);
    ok(r === false, "has(o) after delete returned " + r);
    r = s.get(o2);
    ok(r === "test", "get(o2) after delete returned " + r);
    r = s.has(o2);
    ok(r === true, "has(o2) after delete returned " + r);

    r = s.set(o, undefined);
    ok(r === undefined, "set(o, undefined) returned " + r);
    r = s.get(o);
    ok(r === undefined, "get(o) after re-set returned " + r);
    r = s.has(o);
    ok(r === true, "has(o) after re-set returned " + r);

    r = s.clear();
    ok(r === undefined, "clear() returned " + r);
    r = s.get(o);
    ok(r === undefined, "get(o) after clear returned " + r);
    r = s.has(o);
    ok(r === false, "has(o) after clear returned " + r);
    r = s.get(o2);
    ok(r === undefined, "get(o2) after clear returned " + r);
    r = s.has(o2);
    ok(r === false, "has(o2) after clear returned " + r);

    r = external.newRefTest();
    ok(r.ref === 1, "wrong ref after newRefTest: " + r.ref);
    o = { val: r.get(), map: s };
    s.set(o, o);
    ok(r.ref > 1, "map entry released");

    o = Date.now();
    CollectGarbage();
    function retry() {
        if(r.ref > 1 && Date.now() - o < 5000) {
            CollectGarbage();
            window.setTimeout(retry);
            return;
        }
        ok(r.ref === 1, "map entry not released");
        next_test();
    }
    window.setTimeout(retry);
});

sync_test("storage", function() {
    var v = document.documentMode, i, r, list;

    sessionStorage["add-at-end"] = 0;
    sessionStorage.removeItem("add-at-end");

    sessionStorage.setItem("foobar", "1234");
    ok("foobar" in sessionStorage, "foobar not in sessionStorage");
    r = sessionStorage.foobar;
    ok(r === "1234", "sessionStorage.foobar = " + r);
    sessionStorage.barfoo = 4321;
    r = sessionStorage.getItem("barfoo");
    ok(r === "4321", "sessionStorage.barfoo = " + r);
    sessionStorage.setItem("abcd", "blah");
    sessionStorage.dcba = "test";

    // Order isn't consistent, but changes are reflected during the enumeration.
    // Elements that were already traversed in DISPID (even if removed before
    // the enumeration) are not enumerated, even if re-added during the enum.
    i = 0; list = [ "foobar", "barfoo", "abcd", "dcba" ];
    for(r in sessionStorage) {
        for(var j = 0; j < list.length; j++)
            if(r === list[j])
                break;
        ok(j < list.length, "got '" + r + "' enumerating");
        list.splice(j, 1);
        if(i === 1) {
            sessionStorage.removeItem(list[0]);
            sessionStorage.setItem("new", "new");
            list.splice(0, 1, "new");
        }
        if(!list.length)
            sessionStorage.setItem("add-at-end", "0");
        i++;
    }
    ok(i === 4, "enum did " + i + " iterations");

    try {
        delete sessionStorage.foobar;
        ok(v >= 8, "expected exception deleting sessionStorage.foobar");
        ok(!("foobar" in sessionStorage), "foobar in sessionStorage after deletion");
        r = sessionStorage.getItem("foobar");
        ok(r === null, "sessionStorage.foobar after deletion = " + r);
    }catch(e) {
        ok(v < 8, "did not expect exception deleting sessionStorage.foobar");
        ok(e.number === 0xa01bd - 0x80000000, "deleting sessionStorage.foobar threw = " + e.number);
    }

    sessionStorage.clear();
});

async_test("storage events", function() {
    var iframe = document.createElement("iframe"), iframe2 = document.createElement("iframe");
    var local = false, storage, storage2, v = document.documentMode, i = 0;

    var tests = [
        function() {
            expect();
            storage.removeItem("foobar");
        },
        function() {
            expect(0, "foobar", "", "test");
            storage.setItem("foobar", "test");
        },
        function() {
            expect(1, "foobar", "test", "TEST", true);
            storage2.setItem("foobar", "TEST");
        },
        function() {
            expect(0, "foobar", "TEST", "");
            storage.removeItem("foobar");
        },
        function() {
            expect(1, "winetest", "", "WineTest");
            storage2.setItem("winetest", "WineTest");
        },
        function() {
            expect(0, "", "", "");
            storage.clear();
        }
    ];

    function next() {
        if(++i < tests.length)
            tests[i]();
        else if(local)
            next_test();
        else {
            // w10pro64 testbot VM throws WININET_E_INTERNAL_ERROR for some reason
            storage = null, storage2 = null;
            try {
                storage = window.localStorage, storage2 = iframe.contentWindow.localStorage;
            }catch(e) {
                ok(e.number === 0x72ee4 - 0x80000000, "localStorage threw " + e.number + ": " + e);
            }
            if(!storage || !storage2) {
                win_skip("localStorage is buggy and not available, skipping");
                next_test();
                return;
            }
            i = 0, local = true;

            if(!storage.length)
                setTimeout(function() { tests[0](); });
            else {
                // Get rid of any entries first, since native doesn't update immediately
                var w = [ window, iframe.contentWindow ];
                for(var j = 0; j < w.length; j++)
                    w[j].onstorage = w[j].document.onstorage = w[j].document.onstoragecommit = null;
                document.onstoragecommit = function() {
                    if(!storage.length)
                        setTimeout(function() { tests[0](); });
                    else
                        storage.clear();
                };
                storage.clear();
            }
        }
    }

    function test_event(e, idx, key, oldValue, newValue) {
        if(v < 9) {
            ok(e === undefined, "event not undefined in legacy mode: " + e);
            return;
        }
        var s = Object.prototype.toString.call(e);
        ok(s === "[object StorageEvent]", "Object.toString = " + s);
        ok(e.key === key, "key = " + e.key + ", expected " + key);
        ok(e.oldValue === oldValue, "oldValue = " + e.oldValue + ", expected " + oldValue);
        ok(e.newValue === newValue, "newValue = " + e.newValue + ", expected " + newValue);
        s = (idx ? iframe.contentWindow : window)["location"]["href"].split('#', 1)[0];
        ok(e.url === s, "url = " + e.url + ", expected " + s);
    }

    function expect(idx, key, oldValue, newValue, quirk) {
        var window2 = iframe.contentWindow, document2 = window2.document;
        window.onstorage = function() { ok(false, "window.onstorage called"); };
        document.onstorage = function() { ok(false, "doc.onstorage called"); };
        document.onstoragecommit = function() { ok(false, "doc.onstoragecommit called"); };
        window2.onstorage = function() { ok(false, "iframe window.onstorage called"); };
        document2.onstorage = function() { ok(false, "iframe doc.onstorage called"); };
        document2.onstoragecommit = function() { ok(false, "iframe doc.onstoragecommit called"); };

        if(idx === undefined) {
            setTimeout(function() { next(); });
        }else {
            // Native sometimes calls this for some reason
            if(local && quirk) document.onstoragecommit = null;

            (v < 9 ? document2 : window2)["onstorage"] = function(e) {
                (local && idx ? document2 : (local || v < 9 ? document : window))[local ? "onstoragecommit" : "onstorage"] = function(e) {
                    test_event(e, idx, local ? "" : key, local ? "" : oldValue, local ? "" : newValue);
                    next();
                }
                test_event(e, idx, key, oldValue, newValue);
            }
        }
    }

    iframe.onload = function() {
        iframe2.onload = function() {
            var w = iframe2.contentWindow;
            w.onstorage = function() { ok(false, "about:blank window.onstorage called"); };
            w.document.onstorage = function() { ok(false, "about:blank document.onstorage called"); };
            w.document.onstoragecommit = function() { ok(false, "about:blank document.onstoragecommit called"); };

            storage = window.sessionStorage, storage2 = iframe.contentWindow.sessionStorage;
            tests[0]();
        };
        iframe2.src = "about:blank";
        document.body.appendChild(iframe2);
    };
    iframe.src = "blank.html";
    document.body.appendChild(iframe);
});

sync_test("elem_attr", function() {
    var v = document.documentMode;
    var elem = document.createElement("div"), r;

    function test_exposed(prop, expect) {
        if(expect)
            ok(prop in elem, prop + " is not exposed from elem");
        else
            ok(!(prop in elem), prop + " is exposed from elem");
    }

    r = elem.getAttribute("class");
    ok(r === null, "class attr = " + r);
    r = elem.getAttribute("className");
    ok(r === (v < 8 ? "" : null), "className attr = " + r);

    elem.className = "cls";
    r = elem.getAttribute("class");
    ok(r === (v < 8 ? null : "cls"), "class attr = " + r);
    r = elem.getAttribute("className");
    ok(r === (v < 8 ? "cls" : null), "className attr = " + r);

    elem.setAttribute("class", "cls2");
    ok(elem.className === (v < 8 ? "cls" : "cls2"), "elem.className = " + elem.className);
    r = elem.getAttribute("class");
    ok(r === "cls2", "class attr = " + r);
    r = elem.getAttribute("className");
    ok(r === (v < 8 ? "cls" : null), "className attr = " + r);

    elem.setAttribute("className", "cls3");
    ok(elem.className === (v < 8 ? "cls3" : "cls2"), "elem.className = " + elem.className);
    r = elem.getAttribute("class");
    ok(r === "cls2", "class attr = " + r);
    r = elem.getAttribute("className");
    ok(r === "cls3", "className attr = " + r);

    elem.htmlFor = "for";
    r = elem.getAttribute("for");
    ok(r === null, "for attr = " + r);
    r = elem.getAttribute("htmlFor");
    ok(r === (v < 9 ? "for" : null), "htmlFor attr = " + r);

    elem.setAttribute("for", "for2");
    ok(elem.htmlFor === "for", "elem.htmlFor = " + elem.htmlFor);
    r = elem.getAttribute("for");
    ok(r === "for2", "for attr = " + r);
    r = elem.getAttribute("htmlFor");
    ok(r === (v < 9 ? "for" : null), "htmlFor attr = " + r);

    elem.setAttribute("htmlFor", "for3");
    ok(elem.htmlFor === (v < 9 ? "for3" : "for"), "elem.htmlFor = " + elem.htmlFor);
    r = elem.getAttribute("for");
    ok(r === "for2", "for attr = " + r);
    r = elem.getAttribute("htmlFor");
    ok(r === "for3", "htmlFor attr = " + r);

    elem.setAttribute("testattr", "test", 0, "extra arg", 0xdeadbeef);
    test_exposed("class", v < 8);
    test_exposed("className", true);
    test_exposed("for", v < 9);
    test_exposed("htmlFor", true);
    test_exposed("testattr", v < 9);

    var arr = [3];
    elem.setAttribute("testattr", arr);
    r = elem.getAttribute("testattr");
    ok(r === (v < 8 ? arr : "3"), "testattr = " + r);
    ok(elem.testattr === (v < 9 ? arr : undefined), "elem.testattr = " + elem.testattr);
    r = elem.removeAttribute("testattr");
    ok(r === (v < 9 ? true : undefined), "testattr removeAttribute returned " + r);
    ok(elem.testattr === undefined, "removed testattr = " + elem.testattr);

    arr[0] = 9;
    elem.setAttribute("testattr", "string");
    elem.testattr = arr;
    r = elem.getAttribute("testattr");
    ok(r === (v < 8 ? arr : (v < 9 ? "9" : "string")), "testattr = " + r);
    ok(elem.testattr === arr, "elem.testattr = " + elem.testattr);
    arr[0] = 3;
    r = elem.getAttribute("testattr");
    ok(r === (v < 8 ? arr : (v < 9 ? "3" : "string")), "testattr = " + r);
    ok(elem.testattr === arr, "elem.testattr = " + elem.testattr);
    r = elem.removeAttribute("testattr");
    ok(r === (v < 9 ? true : undefined), "testattr removeAttribute returned " + r);
    ok(elem.testattr === (v < 9 ? undefined : arr), "removed testattr = " + elem.testattr);

    arr.toString = function() { return 42; }
    elem.testattr = arr;
    r = elem.getAttribute("testattr");
    ok(r === (v < 8 ? arr : (v < 9 ? "42" : null)), "testattr with custom toString = " + r);
    elem.setAttribute("testattr", arr);
    r = elem.getAttribute("testattr");
    ok(r === (v < 8 ? arr : "42"), "testattr after setAttribute with custom toString = " + r);
    ok(elem.testattr === arr, "elem.testattr after setAttribute with custom toString = " + elem.testattr);
    r = elem.removeAttribute("testattr");
    ok(r === (v < 9 ? true : undefined), "testattr removeAttribute with custom toString returned " + r);
    ok(elem.testattr === (v < 9 ? undefined : arr), "removed testattr with custom toString = " + elem.testattr);

    arr.valueOf = function() { return "arrval"; }
    elem.testattr = arr;
    r = elem.getAttribute("testattr");
    ok(r === (v < 8 ? arr : (v < 9 ? "arrval" : null)), "testattr with custom valueOf = " + r);
    elem.setAttribute("testattr", arr);
    r = elem.getAttribute("testattr");
    ok(r === (v < 8 ? arr : (v < 10 ? "arrval" : "42")), "testattr after setAttribute with custom valueOf = " + r);
    ok(elem.testattr === arr, "elem.testattr after setAttribute with custom valueOf = " + elem.testattr);
    r = elem.removeAttribute("testattr");
    ok(r === (v < 9 ? true : undefined), "testattr removeAttribute with custom valueOf returned " + r);
    ok(elem.testattr === (v < 9 ? undefined : arr), "removed testattr with custom valueOf = " + elem.testattr);

    var func = elem.setAttribute;
    try {
        func("testattr", arr);
        ok(v < 9, "expected exception setting testattr via func");
    }catch(ex) {
        ok(v >= 9, "did not expect exception setting testattr via func");
        func.call(elem, "testattr", arr);
    }
    r = elem.getAttribute("testattr");
    ok(r === (v < 8 ? arr : (v < 10 ? "arrval" : "42")), "testattr after setAttribute (as func) = " + r);
    delete arr.valueOf;
    delete arr.toString;

    elem.setAttribute("id", arr);
    r = elem.getAttribute("id");
    todo_wine_if(v >= 8 && v < 10).
    ok(r === (v < 8 || v >= 10 ? "3" : "[object]"), "id = " + r);
    r = elem.removeAttribute("id");
    ok(r === (v < 9 ? true : undefined), "id removeAttribute returned " + r);
    ok(elem.id === "", "removed id = " + elem.id);

    func = function() { };
    elem.onclick = func;
    ok(elem.onclick === func, "onclick = " + elem.onclick);
    r = elem.getAttribute("onclick");
    todo_wine_if(v === 8).
    ok(r === (v < 8 ? func : null), "onclick attr = " + r);
    r = elem.removeAttribute("onclick");
    ok(r === (v < 9 ? false : undefined), "removeAttribute returned " + r);
    todo_wine_if(v === 8).
    ok(elem.onclick === (v != 8 ? func : null), "removed onclick = " + elem.onclick);

    elem.onclick_test = func;
    ok(elem.onclick_test === func, "onclick_test = " + elem.onclick_test);
    r = elem.getAttribute("onclick_test");
    ok(r === (v < 8 ? func : (v < 9 ? func.toString() : null)), "onclick_test attr = " + r);

    elem.setAttribute("onclick", "test");
    r = elem.getAttribute("onclick");
    ok(r === "test", "onclick attr after setAttribute = " + r);
    r = elem.removeAttribute("onclick");
    ok(r === (v < 9 ? true : undefined), "removeAttribute after setAttribute returned " + r);

    /* IE11 returns an empty function, which we can't check directly */
    todo_wine_if(v >= 9).
    ok((v < 11) ? (elem.onclick === null) : (elem.onclick !== func), "removed onclick after setAttribute = " + elem.onclick);

    r = Object.prototype.toString.call(elem.onclick);
    todo_wine_if(v >= 9 && v < 11).
    ok(r === (v < 9 ? "[object Object]" : (v < 11 ? "[object Null]" : "[object Function]")),
        "removed onclick after setAttribute Object.toString returned " + r);

    elem.setAttribute("onclick", "string");
    r = elem.getAttribute("onclick");
    ok(r === "string", "onclick attr after setAttribute = " + r);
    elem.onclick = func;
    ok(elem.onclick === func, "onclick = " + elem.onclick);
    r = elem.getAttribute("onclick");
    todo_wine_if(v === 8).
    ok(r === (v < 8 ? func : (v < 9 ? null : "string")), "onclick attr = " + r);
    elem.onclick = "test";
    r = elem.getAttribute("onclick");
    ok(r === (v < 9 ? "test" : "string"), "onclick attr = " + r);
    r = elem.removeAttribute("onclick");
    ok(r === (v < 9 ? true : undefined), "removeAttribute returned " + r);
    todo_wine_if(v >= 9).
    ok(elem.onclick === null, "removed onclick = " + elem.onclick);

    elem.setAttribute("ondblclick", arr);
    r = elem.getAttribute("ondblclick");
    todo_wine_if(v >= 8 && v < 10).
    ok(r === (v < 8 ? arr : (v < 10 ? "[object]" : "3")), "ondblclick = " + r);
    r = elem.removeAttribute("ondblclick");
    ok(r === (v < 8 ? false : (v < 9 ? true : undefined)), "ondblclick removeAttribute returned " + r);
    r = Object.prototype.toString.call(elem.ondblclick);
    todo_wine_if(v >= 11).
    ok(r === (v < 8 ? "[object Array]" : (v < 9 ? "[object Object]" : (v < 11 ? "[object Null]" : "[object Function]"))),
        "removed ondblclick Object.toString returned " + r);

    elem.setAttribute("ondblclick", "string");
    r = elem.getAttribute("ondblclick");
    ok(r === "string", "ondblclick string = " + r);
    r = elem.removeAttribute("ondblclick");
    ok(r === (v < 9 ? true : undefined), "ondblclick string removeAttribute returned " + r);
    ok(elem.ondblclick === null, "removed ondblclick string = " + elem.ondblclick);

    if(v < 9) {
        /* style is a special case */
        try {
            elem.style = "opacity: 1.0";
            ok(false, "expected exception setting elem.style");
        }catch(ex) { }

        var style = elem.style;
        r = elem.getAttribute("style");
        ok(r === (v < 8 ? style : null), "style attr = " + r);
        r = elem.removeAttribute("style");
        ok(r === true, "removeAttribute('style') returned " + r);
        r = elem.style;
        ok(r === style, "removed elem.style = " + r);
        r = elem.getAttribute("style");
        ok(r === (v < 8 ? style : null), "style attr after removal = " + r);
        elem.setAttribute("style", "opacity: 1.0");
        r = elem.getAttribute("style");
        ok(r === (v < 8 ? style : "opacity: 1.0"), "style attr after setAttribute = " + r);
        r = elem.style;
        ok(r === style, "elem.style after setAttribute = " + r);
    }
});

sync_test("elem_attrNS", function() {
    var v = document.documentMode;
    if(v < 9) return;  /* not available */

    var specialspace_ns = "http://www.mozilla.org/ns/specialspace";
    var svg_ns = "http://www.w3.org/2000/svg";

    var elem = document.createElement("div"), r;

    elem.setAttributeNS(specialspace_ns, "spec:align", "left");
    r = elem.hasAttribute("spec:align");
    ok(r === true, "spec:align does not exist");
    r = elem.getAttribute("spec:align");
    ok(r === "left", "spec:align = " + r);
    r = elem.hasAttribute("align");
    ok(r === false, "align exists");
    r = elem.getAttribute("align");
    ok(r === null, "align = " + r);
    r = elem.hasAttributeNS(null, "spec:align");
    ok(r === false, "null spec:align exists");
    r = elem.getAttributeNS(null, "spec:align");
    ok(r === "", "null spec:align = " + r);
    r = elem.hasAttributeNS(null, "spec:align");
    ok(r === false, "null align exists");
    r = elem.getAttributeNS(null, "align");
    ok(r === "", "null align = " + r);
    r = elem.hasAttributeNS(svg_ns, "spec:align");
    ok(r === false, "svg spec:align exists");
    r = elem.getAttributeNS(svg_ns, "spec:align");
    ok(r === "", "svg spec:align = " + r);
    r = elem.hasAttributeNS(svg_ns, "align");
    ok(r === false, "svg align exists");
    r = elem.getAttributeNS(svg_ns, "align");
    ok(r === "", "svg align = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "spec:align");
    ok(r === false, "specialspace spec:align exists");
    r = elem.getAttributeNS(specialspace_ns, "spec:align");
    ok(r === "", "specialspace spec:align = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "align");
    ok(r === true, "specialspace align does not exist");
    r = elem.getAttributeNS(specialspace_ns, "align");
    ok(r === "left", "specialspace align = " + r);

    try {
        elem.setAttributeNS(null, "spec:align", "right");
        ok(false, "expected exception setting qualified attr with null ns");
    }catch(ex) {
        todo_wine.
        ok(ex.message === "NamespaceError", "setAttributeNS(null, 'spec:align', 'right') threw " + ex.message);
    }
    try {
        elem.setAttributeNS("", "spec:align", "right");
        ok(false, "expected exception setting qualified attr with empty ns");
    }catch(ex) {
        todo_wine.
        ok(ex.message === "NamespaceError", "setAttributeNS('', 'spec:align', 'right') threw " + ex.message);
    }
    elem.setAttributeNS(null, "align", "right");
    r = elem.getAttribute("spec:align");
    ok(r === "left", "spec:align (null) = " + r);
    r = elem.hasAttribute("align");
    ok(r === true, "align (null) does not exist");
    r = elem.getAttribute("align");
    ok(r === "right", "align (null) = " + r);
    r = elem.hasAttributeNS(null, "spec:align");
    ok(r === false, "null spec:align exists");
    r = elem.getAttributeNS(null, "spec:align");
    ok(r === "", "null spec:align (null) = " + r);
    r = elem.hasAttributeNS(null, "align");
    ok(r === true, "null align does not exist");
    r = elem.getAttributeNS(null, "align");
    ok(r === "right", "null align (null) = " + r);
    r = elem.hasAttributeNS(svg_ns, "spec:align");
    ok(r === false, "svg spec:align (null) exists");
    r = elem.getAttributeNS(svg_ns, "spec:align");
    ok(r === "", "svg spec:align (null) = " + r);
    r = elem.hasAttributeNS(svg_ns, "align");
    ok(r === false, "svg align (null) exists");
    r = elem.getAttributeNS(svg_ns, "align");
    ok(r === "", "svg align (null) = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "spec:align");
    ok(r === false, "specialspace_ns spec:align (null) exists");
    r = elem.getAttributeNS(specialspace_ns, "spec:align");
    ok(r === "", "specialspace spec:align (null) = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "align");
    ok(r === true, "specialspace align (null) does not exist");
    r = elem.getAttributeNS(specialspace_ns, "align");
    ok(r === "left", "specialspace align (null) = " + r);

    elem.setAttribute("align", "center");
    r = elem.hasAttributeNS(null, "spec:align");
    ok(r === false, "null spec:align (non-NS) exists");
    r = elem.getAttributeNS(null, "spec:align");
    ok(r === "", "null spec:align (non-NS) = " + r);
    r = elem.hasAttributeNS(null, "align");
    ok(r === true, "null align (non-NS) does not exist");
    r = elem.getAttributeNS(null, "align");
    ok(r === "center", "null align (non-NS) = " + r);
    r = elem.hasAttributeNS(svg_ns, "spec:align");
    ok(r === false, "svg spec:align (non-NS) exists");
    r = elem.getAttributeNS(svg_ns, "spec:align");
    ok(r === "", "svg spec:align (non-NS) = " + r);
    r = elem.hasAttributeNS(svg_ns, "align");
    ok(r === false, "svg align (non-NS) exists");
    r = elem.getAttributeNS(svg_ns, "align");
    ok(r === "", "svg align (non-NS) = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "spec:align");
    ok(r === false, "specialspace spec:align (non-NS) exists");
    r = elem.getAttributeNS(specialspace_ns, "spec:align");
    ok(r === "", "specialspace spec:align (non-NS) = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "align");
    ok(r === true, "specialspace align (non-NS) does not exist");
    r = elem.getAttributeNS(specialspace_ns, "align");
    ok(r === "left", "specialspace align (non-NS) = " + r);
    elem.removeAttributeNS(null, "spec:align");

    elem.setAttribute("emptynsattr", "none");
    elem.setAttributeNS("", "emptynsattr", "test");
    r = elem.hasAttribute("emptynsattr");
    ok(r === true, "emptynsattr without NS does not exist");
    r = elem.getAttribute("emptynsattr");
    ok(r === "test", "emptynsattr without NS = " + r);
    elem.setAttributeNS(null, "emptynsattr", "wine");
    r = elem.hasAttribute("emptynsattr");
    ok(r === true, "emptynsattr without NS does not exist");
    r = elem.getAttribute("emptynsattr");
    ok(r === "wine", "emptynsattr without NS = " + r);
    elem.setAttributeNS(specialspace_ns, "emptynsattr", "ns");
    r = elem.hasAttribute("emptynsattr");
    ok(r === true, "emptynsattr without NS does not exist");
    r = elem.getAttribute("emptynsattr");
    ok(r === "wine", "emptynsattr without NS = " + r);
    r = elem.hasAttributeNS("", "emptynsattr");
    ok(r === true, "emptynsattr empty ns does not exist");
    r = elem.getAttributeNS("", "emptynsattr");
    ok(r === "wine", "emptynsattr empty ns = " + r);
    r = elem.hasAttributeNS(null, "emptynsattr");
    ok(r === true, "emptynsattr null ns does not exist");
    r = elem.getAttributeNS(null, "emptynsattr");
    ok(r === "wine", "emptynsattr null ns = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === true, "emptynsattr specialspace ns does not exist");
    r = elem.getAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === "ns", "emptynsattr specialspace ns = " + r);

    elem.removeAttributeNS("", "emptynsattr");
    r = elem.hasAttribute("emptynsattr");
    ok(r === true, "emptynsattr without NS after remove does not exist");
    r = elem.getAttribute("emptynsattr");
    ok(r === "ns", "emptynsattr without NS after remove = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === true, "emptynsattr specialspace ns after empty remove does not exist");
    r = elem.getAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === "ns", "emptynsattr specialspace ns after empty remove = " + r);
    elem.setAttribute("emptynsattr", "test");
    r = elem.getAttribute("emptynsattr");
    ok(r === "test", "emptynsattr without NS after re-set = " + r);
    r = elem.getAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === "test", "emptynsattr specialspace ns after empty re-set = " + r);

    elem.removeAttribute("emptynsattr");
    r = elem.hasAttribute("emptynsattr");
    ok(r === false, "emptynsattr without NS after non-NS remove exists");
    r = elem.getAttribute("emptynsattr");
    ok(r === null, "emptynsattr without NS after non-NS remove = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === false, "emptynsattr specialspace ns after non-NS remove exists");
    r = elem.getAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === "", "emptynsattr specialspace ns after non-NS remove = " + r);

    elem.setAttributeNS(specialspace_ns, "emptynsattr", "ns");
    elem.removeAttributeNS(svg_ns, "emptynsattr");
    r = elem.hasAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === true, "emptynsattr specialspace ns after wrong NS remove does not exist");
    r = elem.getAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === "ns", "emptynsattr specialspace ns after wrong NS remove = " + r);
    r = elem.hasAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === true, "emptynsattr specialspace ns after remove does not exist");
    r = elem.getAttributeNS(specialspace_ns, "emptynsattr");
    ok(r === "ns", "emptynsattr specialspace ns after remove = " + r);

    var ns = {};
    ns.toString = function() { return "toString namespace"; }
    ns.valueOf = function() { return "valueOf namespace"; }
    elem.setAttributeNS(ns, "foobar", "test");
    r = elem.hasAttribute("foobar");
    ok(r === true, "foobar without NS does not exist");
    r = elem.getAttribute("foobar");
    ok(r === "test", "foobar without NS = " + r);
    r = elem.hasAttributeNS(ns, "foobar");
    ok(r === true, "foobar does not exist");
    r = elem.getAttributeNS(ns, "foobar");
    ok(r === "test", "foobar = " + r);
    r = elem.hasAttributeNS("toString namespace", "foobar");
    ok(r === (v < 10 ? false : true), "foobar (toString namespace) " + (v < 10 ? "exists" : "does not exist"));
    r = elem.getAttributeNS("toString namespace", "foobar");
    ok(r === (v < 10 ? "" : "test"), "foobar (toString namespace) = " + r);
    r = elem.hasAttributeNS("valueOf namespace", "foobar");
    ok(r === (v < 10 ? true : false), "foobar (valueOf namespace) = " + (v < 10 ? "does not exist" : "exists"));
    r = elem.getAttributeNS("valueOf namespace", "foobar");
    ok(r === (v < 10 ? "test" : ""), "foobar (valueOf namespace) = " + r);

    var arr = [3];
    elem.setAttributeNS(svg_ns, "testattr", arr);
    r = elem.getAttributeNS(svg_ns, "testattr");
    ok(r === "3", "testattr = " + r);
    ok(elem.testattr === undefined, "elem.testattr = " + elem.testattr);
    elem.removeAttributeNS(svg_ns, "testattr");
    r = elem.getAttributeNS(svg_ns, "testattr");
    ok(r === "", "testattr after remove = " + r);

    arr.toString = function() { return 42; }
    elem.setAttributeNS(svg_ns, "testattr", arr);
    r = elem.getAttributeNS(svg_ns, "testattr");
    ok(r === "42", "testattr with custom toString = " + r);
    elem.removeAttributeNS(svg_ns, "testattr");
    r = elem.getAttributeNS(svg_ns, "testattr");
    ok(r === "", "testattr with custom toString after remove = " + r);

    arr.valueOf = function() { return "arrval"; }
    elem.setAttributeNS(svg_ns, "testattr", arr);
    r = elem.getAttributeNS(svg_ns, "testattr");
    ok(r === "42", "testattr with custom valueOf = " + r);
    elem.removeAttributeNS(svg_ns, "testattr");

    elem.setAttributeNS(svg_ns, "boolattr", true);
    r = elem.getAttributeNS(svg_ns, "boolattr");
    ok(r === "true", "boolattr = " + r);

    elem.setAttributeNS(svg_ns, "numattr", 13);
    r = elem.getAttributeNS(svg_ns, "numattr");
    ok(r === "13", "numattr = " + r);
});

sync_test("builtins_diffs", function() {
    var v = document.documentMode;

    /* despite what spec says for ES6, IE still throws */
    var props = [
        "freeze",
        "getPrototypeOf",
        "isExtensible",
        "isFrozen",
        "isSealed",
        "keys",
        "preventExtensions",
        "seal"
    ];
    for(var i = 0; i < props.length; i++) {
        try {
            Object[props[i]]("test");
            ok(false, "Object." + props[i] + " with non-object: expected exception");
        }catch(e) {
            ok(e.number === (v < 9 ? 0xa01b6 : 0xa138f) - 0x80000000, "Object." + props[i] + " with non-object: exception = " + e.number);
        }
    }

    try {
        RegExp.prototype.toString.call({source: "foo", flags: "g"});
        ok(false, "RegExp.toString with non-regexp: expected exception");
    }catch(e) {
        ok(e.number === 0xa1398 - 0x80000000, "RegExp.toString with non-regexp: exception = " + e.number);
    }
    try {
        RegExp.prototype.toString.call({source: "abc", global: true, ignoreCase: true, multiline: true});
        ok(false, "RegExp.toString with non-regexp 2: expected exception");
    }catch(e) {
        ok(e.number === 0xa1398 - 0x80000000, "RegExp.toString with non-regexp 2: exception = " + e.number);
    }

    try {
        /a/.lastIndex();
        ok(false, "/a/.lastIndex(): expected exception");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "/a/.lastIndex(): exception = " + e.number);
    }
    try {
        "a".length();
        ok(false, "\"a\".length(): expected exception");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "\"a\".length(): exception = " + e.number);
    }
});

sync_test("nullDisp", function() {
    var v = document.documentMode, nullDisp = external.nullDisp, r;

    ok(external.getVT(nullDisp) === "VT_NULL", "getVT(nullDisp) is not VT_NULL");
    ok(typeof(nullDisp) === "object", "typeof(nullDisp) = " + typeof(nullDisp));
    ok(nullDisp === nullDisp, "nullDisp !== nullDisp");
    ok(nullDisp === null, "nullDisp === null");
    ok(nullDisp == null, "nullDisp == null");
    ok(!nullDisp === true, "!nullDisp = " + !nullDisp);
    ok(String(nullDisp) === "null", "String(nullDisp) = " + String(nullDisp));
    ok(+nullDisp === 0, "+nullDisp !== 0");
    ok(''+nullDisp === "null", "''+nullDisp !== null");
    ok(nullDisp != new Object(), "nullDisp == new Object()");
    ok(new Object() != nullDisp, "new Object() == nullDisp");
    ok((typeof Object(nullDisp)) === "object", "typeof Object(nullDisp) !== 'object'");
    r = Object(nullDisp).toString();
    ok(r === "[object Object]", "Object(nullDisp).toString() = " + r);
    ok(Object(nullDisp) != nullDisp, "Object(nullDisp) == nullDisp");
    ok(new Object(nullDisp) != nullDisp, "new Object(nullDisp) == nullDisp");
    r = (nullDisp instanceof Object);
    ok(r === false, "nullDisp instance of Object");

    if(v >= 8) {
        r = JSON.stringify.call(null, nullDisp);
        ok(r === "null", "JSON.stringify(nullDisp) returned " + r);
    }

    try {
        (new Object()) instanceof nullDisp;
        ok(false, "expected exception on (new Object()) instanceof nullDisp");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "(new Object()) instanceof nullDisp threw " + e.number);
    }

    try {
        Function.prototype.apply.call(nullDisp, Object, []);
        ok(false, "expected exception calling Function.apply on nullDisp");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "Function.apply on nullDisp threw " + e.number);
    }
    try {
        Function.prototype.call.call(nullDisp, Object);
        ok(false, "expected exception calling Function.call on nullDisp");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "Function.call on nullDisp threw " + e.number);
    }

    try {
        new nullDisp;
        ok(false, "expected exception for new nullDisp");
    }catch(e) {
        ok(e.number === 0xa138f - 0x80000000, "new nullDisp threw " + e.number);
    }
});

sync_test("invalid selectors", function() {
    var v = document.documentMode, body = document.body, i;
    if(v < 8)
        return;

    var selectors = [
        "[s!='']",
        "*,:x",
        "*,##",
        ":x",
        "##",
        "*,",
        ","
    ];

    for(i = 0; i < selectors.length; i++) {
        try {
            body.querySelector(selectors[i]);
            ok(false, "body.querySelector(\"" + selectors[i] + "\" did not throw exception");
        }catch(e) {
            if(v < 9)
                ok(e.number === 0x70057 - 0x80000000, "body.querySelector(\"" + selectors[i] + "\" threw " + e.number);
            else {
                todo_wine.
                ok(e.name === (v < 10 ? undefined : "SyntaxError"), "body.querySelector(\"" + selectors[i] + "\" threw " + e.name);
            }
        }
        try {
            body.querySelectorAll(selectors[i]);
            ok(false, "body.querySelectorAll(\"" + selectors[i] + "\" did not throw exception");
        }catch(e) {
            if(v < 9)
                ok(e.number === 0x70057 - 0x80000000, "body.querySelectorAll(\"" + selectors[i] + "\" threw " + e.number);
            else {
                todo_wine.
                ok(e.name === (v < 10 ? undefined : "SyntaxError"), "body.querySelectorAll(\"" + selectors[i] + "\" threw " + e.name);
            }
        }
    }

    if(!body.msMatchesSelector)
        return;

    for(i = 0; i < selectors.length; i++) {
        try {
            body.msMatchesSelector(selectors[i]);
            ok(false, "body.msMatchesSelector(\"" + selectors[i] + "\" did not throw exception");
        }catch(e) {
            if(v < 9)
                ok(e.number === 0x70057 - 0x80000000, "body.msMatchesSelector(\"" + selectors[i] + "\" threw " + e.number);
            else {
                todo_wine.
                ok(e.name === (v < 10 ? undefined : "SyntaxError"), "body.msMatchesSelector(\"" + selectors[i] + "\" threw " + e.name);
            }
        }
    }
});

sync_test("__proto__", function() {
    var v = document.documentMode;
    var r, x = 42;

    if(v < 11) {
        ok(x.__proto__ === undefined, "x.__proto__ = " + x.__proto__);
        ok(!("__proto__" in Object), "Object.__proto__ = " + Object.__proto__);
        return;
    }

    ok(x.__proto__ === Number.prototype, "x.__proto__ = " + x.__proto__);
    ok(Object.__proto__ === Function.prototype, "Object.__proto__ = " + Object.__proto__);
    ok(Object.prototype.__proto__ === null, "Object.prototype.__proto__ = " + Object.prototype.__proto__);
    ok(Object.prototype.hasOwnProperty("__proto__"), "__proto__ is not a property of Object.prototype");
    ok(!Object.prototype.hasOwnProperty.call(x, "__proto__"), "__proto__ is a property of x");

    x.__proto__ = Object.prototype;
    ok(x.__proto__ === Number.prototype, "x.__proto__ set to Object.prototype = " + x.__proto__);
    ok(!Object.prototype.hasOwnProperty.call(x, "__proto__"), "__proto__ is a property of x after set to Object.prototype");
    x = {};
    x.__proto__ = null;
    r = Object.getPrototypeOf(x);
    ok(x.__proto__ === undefined, "x.__proto__ after set to null = " + x.__proto__);
    ok(r === null, "getPrototypeOf(x) after set to null = " + r);

    function check(expect, msg) {
        var r = Object.getPrototypeOf(x);
        ok(x.__proto__ === expect, "x.__proto__ " + msg + " = " + x.__proto__);
        ok(r === expect, "getPrototypeOf(x) " + msg + " = " + r);
        ok(!Object.prototype.hasOwnProperty.call(x, "__proto__"), "__proto__ is a property of x " + msg);
    }

    x = {};
    check(Object.prototype, "after x set to {}");
    x.__proto__ = Number.prototype;
    check(Number.prototype, "after set to Number.prototype");
    x.__proto__ = Object.prototype;
    check(Object.prototype, "after re-set to Object.prototype");

    function ctor() { }
    var obj = new ctor();
    x.__proto__ = obj;
    check(obj, "after set to obj");
    x.__proto__ = ctor.prototype;
    check(obj.__proto__, "after set to ctor.prototype");
    ok(obj.__proto__ === ctor.prototype, "obj.__proto__ !== ctor.prototype");

    r = (delete x.__proto__);
    ok(r, "delete x.__proto__ returned " + r);
    ok(Object.prototype.hasOwnProperty("__proto__"), "__proto__ is not a property of Object.prototype after delete");
    r = Object.getPrototypeOf(x);
    ok(r === ctor.prototype, "x.__proto__ after delete = " + r);

    var desc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
    ok(desc.value === undefined, "__proto__ value = " + desc.value);
    ok(Object.getPrototypeOf(desc.get) === Function.prototype, "__proto__ getter not a function");
    ok(Object.getPrototypeOf(desc.set) === Function.prototype, "__proto__ setter not a function");
    ok(desc.get.length === 0, "__proto__ getter length = " + desc.get.length);
    ok(desc.set.length === 1, "__proto__ setter length = " + desc.set.length);

    r = desc.get.call(x, 1, 2, 3, 4);
    ok(r === x.__proto__, "calling __proto__ getter on x returned " + r);

    r = desc.set.call(x, obj);
    ok(r === obj, "calling __proto__ setter(obj) on x returned " + r);
    check(obj, "after set to obj via calling setter");
    r = desc.set.call(x, 42);
    ok(r === 42, "calling __proto__ setter(42) on x returned " + r);
    check(obj, "after set to obj via calling setter(42)");
    r = desc.set.call(x, "foo");
    ok(r === "foo", "calling __proto__ setter('foo') on x returned " + r);
    check(obj, "after set to obj via calling setter('foo')");
    r = desc.set.call(x);
    ok(r === undefined, "calling __proto__ setter() on x returned " + r);
    r = desc.set.call(true, obj);
    ok(r === obj, "calling __proto__ setter(obj) on true value returned " + r);
    x = true;
    r = desc.set.call(x, obj);
    ok(r === obj, "calling __proto__ setter(obj) on x set to true returned " + r);
    ok(x.__proto__ === Boolean.prototype, "true value __proto__ after set to obj = " + x.__proto__);
    x = new Boolean(true);
    r = desc.set.call(x, obj);
    ok(r === obj, "calling __proto__ setter(obj) on x set to Boolean(true) returned " + r);
    ok(x.__proto__ === obj, "Boolean(true) __proto__ after set to obj = " + x.__proto__);

    r = desc.get.call(13);
    ok(r === Number.prototype, "calling __proto__ getter on 13 returned " + r);
    try {
        r = desc.get.call(undefined);
        ok(false, "expected exception calling __proto__ getter on undefined");
    }catch(e) {
        ok(e.number === 0xa138f - 0x80000000, "calling __proto__ getter on undefined threw exception " + e.number);
    }
    try {
        r = desc.get.call(null);
        ok(false, "expected exception calling __proto__ getter on null");
    }catch(e) {
        ok(e.number === 0xa138f - 0x80000000, "calling __proto__ getter on null threw exception " + e.number);
    }

    try {
        r = desc.set.call(undefined, obj);
        ok(false, "expected exception calling __proto__ setter on undefined");
    }catch(e) {
        ok(e.number === 0xa138f - 0x80000000, "calling __proto__ setter on undefined threw exception " + e.number);
    }
    try {
        r = desc.set.call(null, obj);
        ok(false, "expected exception calling __proto__ setter on null");
    }catch(e) {
        ok(e.number === 0xa138f - 0x80000000, "calling __proto__ setter on null threw exception " + e.number);
    }

    x = {};
    r = Object.create(x);
    ok(r.__proto__ === x, "r.__proto__ = " + r.__proto__);
    r = Object.create(r);
    ok(r.__proto__.__proto__ === x, "r.__proto__.__proto__ = " + r.__proto__.__proto__);
    try {
        x.__proto__ = r;
        ok(false, "expected exception setting circular proto chain");
    }catch(e) {
        ok(e.number === 0xa13b0 - 0x80000000 && e.name === "TypeError",
            "setting circular proto chain threw exception " + e.number + " (" + e.name + ")");
    }

    Object.preventExtensions(x);
    x.__proto__ = Object.prototype;  /* same prototype */
    try {
        x.__proto__ = Number.prototype;
        ok(false, "expected exception changing __proto__ on non-extensible object");
    }catch(e) {
        ok(e.number === 0xa13b6 - 0x80000000 && e.name === "TypeError",
            "changing __proto__ on non-extensible object threw exception " + e.number + " (" + e.name + ")");
    }

    obj = document.createElement("img");
    obj.__proto__ = ctor.prototype;
    document.body.setAttribute.call(obj, "height", "101");
    r = document.body.getAttribute.call(obj, "height");
    ok(r === "101", "getAttribute(height) = " + r);
    ok(!("getAttribute" in obj), "getAttribute exposed in obj");
});

sync_test("__defineGetter__", function() {
    var v = document.documentMode;
    var r, x = 42;

    if(v < 11) {
        ok(x.__defineGetter__ === undefined, "x.__defineGetter__ = " + x.__defineGetter__);
        ok(!("__defineGetter__" in Object), "Object.__defineGetter__ = " + Object.__defineGetter__);
        return;
    }
    ok(Object.prototype.hasOwnProperty("__defineGetter__"), "__defineGetter__ is not a property of Object.prototype");
    ok(Object.prototype.__defineGetter__.length === 2, "__defineGetter__.length = " + Object.prototype.__defineGetter__.length);

    function getter() { return "wine"; }
    function setter(val) { }

    r = x.__defineGetter__("foo", getter);
    ok(r === undefined, "__defineGetter__ on 42 returned " + r);
    ok(x.foo === undefined, "42.foo = " + x.foo);

    x = {};
    r = x.__defineGetter__("foo", getter);
    ok(r === undefined, "__defineGetter__ returned " + r);
    ok(x.foo === "wine", "x.foo = " + x.foo);
    r = Object.getOwnPropertyDescriptor(x, "foo");
    ok(r.value === undefined, "x.foo value = " + r.value);
    ok(r.get === getter, "x.foo get = " + r.get);
    ok(r.set === undefined, "x.foo set = " + r.set);
    ok(r.writable === undefined, "x.foo writable = " + r.writable);
    ok(r.enumerable === true, "x.foo enumerable = " + r.enumerable);
    ok(r.configurable === true, "x.foo configurable = " + r.configurable);

    Object.defineProperty(x, "foo", { get: undefined, set: setter, configurable: false });
    r = Object.getOwnPropertyDescriptor(x, "foo");
    ok(r.value === undefined, "x.foo setter value = " + r.value);
    ok(r.get === undefined, "x.foo setter get = " + r.get);
    ok(r.set === setter, "x.foo setter set = " + r.set);
    ok(r.writable === undefined, "x.foo setter writable = " + r.writable);
    ok(r.enumerable === true, "x.foo setter enumerable = " + r.enumerable);
    ok(r.configurable === false, "x.foo setter configurable = " + r.configurable);
    try {
        x.__defineGetter__("foo", getter);
        ok(false, "expected exception calling __defineGetter__ on non-configurable property");
    }catch(e) {
        ok(e.number === 0xa13d6 - 0x80000000, "__defineGetter__ on non-configurable property threw exception " + e.number);
    }

    r = Object.prototype.__defineGetter__.call(undefined, "bar", getter);
    ok(r === undefined, "__defineGetter__ on undefined returned " + r);
    r = Object.prototype.__defineGetter__.call(null, "bar", getter);
    ok(r === undefined, "__defineGetter__ on null returned " + r);
    r = x.__defineGetter__(undefined, getter);
    ok(r === undefined, "__defineGetter__ undefined prop returned " + r);
    ok(x["undefined"] === "wine", "x.undefined = " + x["undefined"]);
    r = x.__defineGetter__(false, getter);
    ok(r === undefined, "__defineGetter__ undefined prop returned " + r);
    ok(x["false"] === "wine", "x.false = " + x["false"]);

    try {
        x.__defineGetter__("bar", "string");
        ok(false, "expected exception calling __defineGetter__ with string");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "__defineGetter__ with string threw exception " + e.number);
    }
    try {
        x.__defineGetter__("bar", undefined);
        ok(false, "expected exception calling __defineGetter__ with undefined");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "__defineGetter__ with undefined threw exception " + e.number);
    }
    try {
        x.__defineGetter__("bar", null);
        ok(false, "expected exception calling __defineGetter__ with null");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "__defineGetter__ with null threw exception " + e.number);
    }
    try {
        Object.prototype.__defineGetter__.call(x, "bar");
        ok(false, "expected exception calling __defineGetter__ with only one arg");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "__defineGetter__ with only one arg threw exception " + e.number);
    }

    x.bar = "test";
    ok(x.bar === "test", "x.bar = " + x.bar);
    x.__defineGetter__("bar", getter);
    ok(x.bar === "wine", "x.bar with getter = " + x.bar);
});

sync_test("__defineSetter__", function() {
    var v = document.documentMode;
    var r, x = 42;

    if(v < 11) {
        ok(x.__defineSetter__ === undefined, "x.__defineSetter__ = " + x.__defineSetter__);
        ok(!("__defineSetter__" in Object), "Object.__defineSetter__ = " + Object.__defineSetter__);
        return;
    }
    ok(Object.prototype.hasOwnProperty("__defineSetter__"), "__defineSetter__ is not a property of Object.prototype");
    ok(Object.prototype.__defineSetter__.length === 2, "__defineSetter__.length = " + Object.prototype.__defineSetter__.length);

    function getter() { return "wine"; }
    function setter(val) { this.setterVal = val - 1; }

    r = x.__defineSetter__("foo", setter);
    ok(r === undefined, "__defineSetter__ on 42 returned " + r);
    ok(x.foo === undefined, "42.foo = " + x.foo);

    x = {};
    r = x.__defineSetter__("foo", setter);
    ok(r === undefined, "__defineSetter__ returned " + r);
    ok(x.setterVal === undefined, "x.setterVal = " + x.setterVal);
    x.foo = 13;
    ok(x.setterVal === 12, "x.setterVal = " + x.setterVal);
    r = Object.getOwnPropertyDescriptor(x, "foo");
    ok(r.value === undefined, "x.foo value = " + r.value);
    ok(r.get === undefined, "x.foo get = " + r.get);
    ok(r.set === setter, "x.foo set = " + r.set);
    ok(r.writable === undefined, "x.foo writable = " + r.writable);
    ok(r.enumerable === true, "x.foo enumerable = " + r.enumerable);
    ok(r.configurable === true, "x.foo configurable = " + r.configurable);

    Object.defineProperty(x, "foo", { get: getter, set: undefined, configurable: false });
    r = Object.getOwnPropertyDescriptor(x, "foo");
    ok(r.value === undefined, "x.foo getter value = " + r.value);
    ok(r.get === getter, "x.foo getter get = " + r.get);
    ok(r.set === undefined, "x.foo getter set = " + r.set);
    ok(r.writable === undefined, "x.foo getter writable = " + r.writable);
    ok(r.enumerable === true, "x.foo getter enumerable = " + r.enumerable);
    ok(r.configurable === false, "x.foo getter configurable = " + r.configurable);
    try {
        x.__defineSetter__("foo", setter);
        ok(false, "expected exception calling __defineSetter__ on non-configurable property");
    }catch(e) {
        ok(e.number === 0xa13d6 - 0x80000000, "__defineSetter__ on non-configurable property threw exception " + e.number);
    }

    r = Object.prototype.__defineSetter__.call(undefined, "bar", setter);
    ok(r === undefined, "__defineSetter__ on undefined returned " + r);
    r = Object.prototype.__defineSetter__.call(null, "bar", setter);
    ok(r === undefined, "__defineSetter__ on null returned " + r);
    r = x.__defineSetter__(null, setter);
    ok(r === undefined, "__defineSetter__ null prop returned " + r);
    x["null"] = 100;
    ok(x.setterVal === 99, "x.setterVal after setting x.null = " + x.setterVal);
    r = x.__defineSetter__(50, setter);
    ok(r === undefined, "__defineSetter__ 50 prop returned " + r);
    x["50"] = 33;
    ok(x.setterVal === 32, "x.setterVal after setting x.50 = " + x.setterVal);

    try {
        x.__defineSetter__("bar", true);
        ok(false, "expected exception calling __defineSetter__ with bool");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "__defineSetter__ with bool threw exception " + e.number);
    }
    try {
        x.__defineSetter__("bar", undefined);
        ok(false, "expected exception calling __defineSetter__ with undefined");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "__defineSetter__ with undefined threw exception " + e.number);
    }
    try {
        x.__defineSetter__("bar", null);
        ok(false, "expected exception calling __defineSetter__ with null");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "__defineSetter__ with null threw exception " + e.number);
    }
    try {
        Object.prototype.__defineSetter__.call(x, "bar");
        ok(false, "expected exception calling __defineSetter__ with only one arg");
    }catch(e) {
        ok(e.number === 0xa138a - 0x80000000, "__defineSetter__ with only one arg threw exception " + e.number);
    }

    x.bar = "test";
    ok(x.bar === "test", "x.bar = " + x.bar);
    x.__defineSetter__("bar", setter);
    ok(x.bar === undefined, "x.bar with setter = " + x.bar);
    x.bar = 10;
    ok(x.bar === undefined, "x.bar with setter = " + x.bar);
    ok(x.setterVal === 9, "x.setterVal after setting bar = " + x.setterVal);
});

sync_test("Crypto", function() {
    var crypto = window.msCrypto, r;
    if(!crypto) return;

    ok(Object.prototype.hasOwnProperty.call(Object.getPrototypeOf(window), "msCrypto"), "msCrypto not a property of window's prototype.");
    r = Object.getPrototypeOf(crypto);
    ok(r === window.Crypto.prototype, "getPrototypeOf(crypto) = " + r);

    ok("subtle" in crypto, "subtle not in crypto");
    ok("getRandomValues" in crypto, "getRandomValues not in crypto");
    ok(!("randomUUID" in crypto), "randomUUID is in crypto");
});

sync_test("MutationObserver", function() {
    if (!window.MutationObserver) {
        return;
    }

    try {
        window.MutationObserver();
        ok(false, "MutationObserver without args should fail");
    } catch(e) {
        ok(e.number == 0xffff - 0x80000000, "MutationObserver without new threw exception " + e.number);
    }

    try {
        window.MutationObserver(42);
        ok(false, "MutationObserver with non-function should fail");
    } catch(e) {
        todo_wine.
        ok(e.name == "TypeMismatchError", "MutationObserver with non-function arg threw exception " + e.name);
    }

    try {
        window.MutationObserver(function() {});
    } catch(e) {
        ok(false, "MutationObserver without new threw exception " + e.number);
    }

    try {
        new window.MutationObserver();
        ok(false, "MutationObserver with no args should fail");
    } catch(e) {
        ok(e.number == 0xffff - 0x80000000, "MutationObserver with no args threw exception " + e.number);
    }

    try {
        new window.MutationObserver(1);
        ok(false, "MutationObserver with non-function arg should fail");
    } catch(e) {
        todo_wine.
        ok(e.name == "TypeMismatchError", "MutationObserver with non-function arg threw exception " + e.name);
    }

    try {
        new window.MutationObserver(function() {});
    } catch(e) {
        ok(false, "MutationObserver threw exception " + e.number);
    }

    try {
        new window.MutationObserver(function() {}, 1);
    } catch(e) {
        ok(false, "MutationObserver with extra args threw exception " + e.number);
    }

    var mutation_observer = new MutationObserver(function() {});
    function test_exposed(prop) {
        ok(prop in mutation_observer, prop + " not found in MutationObserver.");
    }
    test_exposed("observe");
    test_exposed("disconnect");
    test_exposed("takeRecords");
});

sync_test("initMessageEvent", function() {
    var e, v = document.documentMode;
    if(!document.createEvent)
        return;
    e = document.createEvent("MessageEvent");
    ok(e.data === (v < 10 ? "" : undefined), "e.data = " + e.data);
    ok(e.bubbles === false, "bubbles = " + e.bubbles);
    ok(e.cancelable === false, "cancelable = " + e.cancelable);
    ok(e.source === null, "e.source = " + e.source);
    ok(e.origin === "", "e.origin = " + e.origin);

    e.initMessageEvent("blah", true, true, 137, "wine", 1234, window);
    ok(e.data === "137", "e.data = " + e.data);
    ok(e.bubbles === true, "bubbles = " + e.bubbles);
    ok(e.cancelable === true, "cancelable = " + e.cancelable);
    ok(e.source === window, "e.source = " + e.source);
    ok(e.origin === "wine", "e.origin = " + e.origin);

    e.initMessageEvent("abcd", false, false, "testdata", "origin", 42, null);
    ok(e.data === "testdata", "e.data = " + e.data);
    ok(e.bubbles === false, "bubbles = " + e.bubbles);
    ok(e.cancelable === false, "cancelable = " + e.cancelable);
    ok(e.source === null, "e.source = " + e.source);
    ok(e.origin === "origin", "e.origin = " + e.origin);
});

async_test("postMessage", function() {
    var v = document.documentMode;
    var onmessage_called = false;
    window.onmessage = function(e) {
        onmessage_called = true;
        if(v < 9)
            ok(e === undefined, "e = " + e);
        else {
            ok(e.data === (v < 10 ? "10" : 10), "e.data = " + e.data);
            ok(e.source === window, "e.source = " + e.source);
            ok(e.origin === "http://winetest.example.org", "e.origin = " + e.origin);

            e = document.createEvent("MessageEvent");
            ok(e.data === (v < 10 ? "" : undefined), "created e.data = " + e.data);
            next_test();
        }
    }

    var invalid = [
        v < 10 ? { toString: function() { return "http://winetest.example.org"; } } : null,
        (function() { return "http://winetest.example.org"; }),
        "winetest.example.org",
        "example.org",
        undefined
    ];
    for(var i = 0; i < invalid.length; i++) {
        try {
            window.postMessage("invalid " + i, invalid[i]);
            ok(false, "expected exception with targetOrigin " + invalid[i]);
        }catch(ex) {
            var n = ex.number >>> 0;
            todo_wine_if(v >= 10).
            ok(n === (v < 10 ? 0x80070057 : 0), "postMessage with targetOrigin " + invalid[i] + " threw " + n);
            if(v >= 10)
                todo_wine.
                ok(ex.name === "SyntaxError", "postMessage with targetOrigin " + invalid[i] + " threw " + ex.name);
        }
    }
    try {
        window.postMessage("invalid empty", "");
        ok(false, "expected exception with empty targetOrigin");
    }catch(ex) {
        var n = ex.number >>> 0;
        ok(n === 0x80070057, "postMessage with empty targetOrigin threw " + n);
    }

    window.postMessage("wrong port", "http://winetest.example.org:1234");
    ok(onmessage_called == (v < 9 ? true : false), "onmessage not called with wrong port");
    onmessage_called = false;

    var not_sent = [
        "http://winetest.example.com",
        "ftp://winetest.example.org",
        "http://wine.example.org",
        "http://example.org"
    ];
    for(var i = 0; i < not_sent.length; i++) {
        window.postMessage("not_sent " + i, not_sent[i]);
        ok(onmessage_called == false, "onmessage called with targetOrigin " + not_sent[i]);
        onmessage_called = false;
    }

    window.postMessage(10, (v < 10 ? "*" : { toString: function() { return "*"; } }));
    ok(onmessage_called == (v < 9 ? true : false), "onmessage not called");
    if(v < 9) next_test();
});
