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

function test_xhr() {
    var xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<a name=\"test\">wine</a>";
    var xhr = new XMLHttpRequest();
    var v = document.documentMode;
    var complete_cnt = 0;

    xhr.onreadystatechange = function() {
        if(xhr.readyState != 4)
            return;

        ok(xhr.responseText === xml, "unexpected responseText " + xhr.responseText);

        var x = xhr.responseXML, r = Object.prototype.toString.call(x);
        ok(r === (v < 10 ? "[object Object]" : (v < 11 ? "[object Document]" : "[object XMLDocument]")),
                "XML document Object.toString = " + r);

        r = Object.getPrototypeOf(x);
        if(v < 10)
            ok(r === null, "prototype of returned XML document = " + r);
        else if(v < 11)
            ok(r === window.Document.prototype, "prototype of returned XML document = " + r);
        else
            ok(r === window.XMLDocument.prototype, "prototype of returned XML document" + r);

        if(v < 10) {
            ok(!("anchors" in x), "anchors is in returned XML document");
            ok(Object.prototype.hasOwnProperty.call(x, "createElement"), "createElement not a prop of returned XML document");
        }else {
            ok("anchors" in x, "anchors not in returned XML document");
            ok(!x.hasOwnProperty("createElement"), "createElement is a prop of returned XML document");
            r = x.anchors;
            ok(r.length === 0, "anchors.length of returned XML document = " + r.length);
        }

        if(complete_cnt++)
            next_test();
    }
    var onload_func = xhr.onload = function() {
        ok(xhr.statusText === "OK", "statusText = " + xhr.statusText);
        if(complete_cnt++)
            next_test();
    };
    ok(xhr.onload === onload_func, "xhr.onload != onload_func");

    xhr.open("POST", "echo.php", true);
    xhr.setRequestHeader("X-Test", "True");
    xhr.send(xml);
}

var tests = [
    test_xhr
];
