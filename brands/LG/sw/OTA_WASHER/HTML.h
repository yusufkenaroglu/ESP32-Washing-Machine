/*
 * HTML.h
 * 
 * Copyright 2025 Yusuf Emre Kenaroglu
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
const char PAGE_MAIN[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en" class="js-focus-visible">
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LG FUZZY OTA WEB UI WASHER</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/flickity/2.2.2/flickity.min.css">
    <style>
        html,
        body {
            height: 100%;
            margin: 0;
        }
        body {
            background-image: url("https://i.pinimg.com/736x/1e/df/63/1edf63c0acd93fc7f3812ca7e2a3f192.jpg");
            color: #E0E0E0;
            background-repeat: no-repeat;
            background-position: top;
            display: flex-grow;
            display: flex;
            flex-direction: column;
        }
        table {
            width: 100%;
        }
        tr {
            border: 1px solid #424242;
            font-family: "Verdana", "Arial", sans-serif;
            font-size: 20px;
        }
        th {
            height: 20px;
            padding: 3px 15px;
            background-color: #212121;
            color: #FFFFFF !important;
        }
        td {
            height: 20px;
            padding: 3px 15px;
        }
        .tabledata {
            font-size: 24px;
            position: relative;
            padding-left: 5px;
            padding-top: 5px;
            height: 25px;
            border-radius: 5px;
            color: #FFFFFF;
            line-height: 20px;
            transition: all 200ms ease-in-out;
            background-color: #00AA00;
        }
        .fanrpmslider {
            width: 30%;
            height: 55px;
            outline: none;
            height: 25px;
        }
        .bodytext {
            font-family: "Verdana", "Arial", sans-serif;
            font-size: 20px;
            text-align: left;
            font-weight: light;
            border-radius: 5px;
            display: inline;
            color: #E0E0E0;
        }
        .navbar {
            width: 100%;
            height: 50px;
            backdrop-filter: blur(10px);
            -webkit-backdrop-filter: blur(10px);
            color: #E0E0E0;
            border-bottom: 5px solid #C70851;
        }
        .fixed-top {
            position: fixed;
            top: 0;
            right: 0;
            left: 0;
            z-index: 1;
        }
        .navtitle {
            height: 50px;
            font-family: "Verdana", "Arial", sans-serif;
            font-size: 25px;
            font-weight: bold;
            line-height: 50px;
            padding-left: 20px;
        }
        .navheading {
            position: fixed;
            left: 60%;
            height: 50px;
            font-family: "Verdana", "Arial", sans-serif;
            font-size: 20px;
            font-weight: bold;
            line-height: 20px;
            padding-right: 20px;
        }
        .navdata {
            justify-content: flex-end;
            position: fixed;
            left: 70%;
            height: 50px;
            font-family: "Verdana", "Arial", sans-serif;
            font-size: 14px;
            font-weight: bold;
            line-height: 20px;
            padding-right: 20px;
        }
        .category {
            font-family: "Verdana", "Arial", sans-serif;
            font-weight: bold;
            font-size: 20px;
            line-height: 20px;
            padding: 5px 5px 0px 10px;
            color: #E0E0E0;
        }
        .heading {
            font-family: "Verdana", "Arial", sans-serif;
            font-weight: normal;
            font-size: 28px;
            text-align: left;
        }
        .btn {
            border-radius: 60px;
            background: linear-gradient(145deg, #1e1e1e, #232323);
            box-shadow: 12px 12px 15px #161616,
                -12px -12px 15px #2c2c2c;
            border-style: hidden;
            color: white;
            padding: 20px 20px;
            display: inline-block;
            font-size: 16px;
            margin: 4px 20px;
        }
        .btn-power::before {
            content: "\23FC";
            /* Unicode for power symbol */
            font-size: 24px;
        }
        .btn-startstop::before {
            content: "\25B6";
            /* Unicode for play symbol */
            font-size: 24px;
        }
        .container {
            max-width: 100%;
            margin: 0 auto;
        }
        table tr:first-child th:first-child {
            border-top-left-radius: 5px;
        }
        table tr:first-child th:last-child {
            border-top-right-radius: 5px;
        }
        table tr:last-child td:first-child {
            border-bottom-left-radius: 5px;
        }
        table tr:last-child td:last-child {
            border-bottom-right-radius: 5px;
        }
        .program-selector {
            border-radius: 10px;
            font-family: "Verdana", "Arial", sans-serif;
            box-shadow: 0 2px 6px rgba(0, 0, 0, 0.5);
            margin-top: 6rem;
            display: block;
            /* Initially hide the program selector */
        }
        .carousel {
            margin-top: 20px;
        }
        .carousel-cell {
            transition: all 0.3s ease-in-out;
            width: 75%;
            height: 400px;
            color: white;
            display: flex;
            justify-content: center;
            align-items: center;
            border-radius: 10px;
            cursor: pointer;
            transform: scale(0.8);
            background: rgba(255, 255, 255, 0.06);
            box-shadow: 0 4px 30px rgba(0, 0, 0, 0.1);
            backdrop-filter: blur(11.6px);
            -webkit-backdrop-filter: blur(11.6px);
            border: 1px solid rgba(255, 255, 255, 0.27);
            font-size: 1.2em;
            padding-top: 0; /* Initial value */
            padding-left: 0; /* Initial value */
        }
        .carousel-cell.is-selected {
            transition: all 0.3s ease-in-out;
            justify-content: flex-start;
            align-items: flex-start;
            padding-top: 10px;
            padding-left: 10px;
            z-index: 1;
            transform: scale(0.9);
            background: rgba(255, 255, 255, 0.16);
            box-shadow: 0 4px 30px rgba(0, 0, 0, 0.1);
            backdrop-filter: blur(11.6px);
            -webkit-backdrop-filter: blur(11.6px);
            border: 1px solid rgba(255, 255, 255, 0.27);
            font-size: 1.5em;
        }
        .loader-container {
            display: flex;
            justify-content: center;
            align-items: center;
            width: 100%;
            height: 100%;
        }
        .loader {
            position: relative;
            width: 180px;
            height: 180px;
            border: 5px solid rgba(39, 39, 39, 0.23);
            border-radius: 50%;
            box-shadow: -5px -5px 5px rgba(0, 0, 0, 0.1),
                10px 10px 10px rgba(0, 0, 0, 0.4),
                inset -5px -5px 5px rgba(0, 0, 0, 0.2),
                inset 10px 10px 10px rgba(0, 0, 0, 0.4);

        }
        .loader:before {
            content: "";
            position: absolute;
            z-index: 10;
            opacity: 80%;
            background: #212121;
            border-radius: 50%;
            border: 2px solid #292929;
            box-shadow: inset -2px -2px 5px rgba(255, 255, 255, 0.2),
                inset 3px 3px 5px rgba(0, 0, 0, 0.5);

        }
        .loader span {
            position: absolute;
            width: 100%;
            height: 100%;
            border-radius: 50%;
            box-shadow: 0 4px 30px rgba(0, 0, 0, 0.1);
            backdrop-filter: blur(11.6px);
            -webkit-backdrop-filter: blur(11.6px);
            border: 1px solid rgba(255, 255, 255, 0.27);
            filter: blur(20px);
            z-index: -1;

        }
        .loader-text {
            font-family: "Verdana", "Arial", sans-serif;
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            text-align: center;
            font-weight: bold;
            font-size: 22px;
            color: #efefef;
        }
    </style>
</head>
<body onload="process()">
    <header>
        <div class="navbar fixed-top">
            <div class="container">
                <div class="navtitle">LG BNK8000H0E</div>
            </div>
        </div>
    </header>
    <main class="container" style="margin-top:70px">
        <div class="bodytext">Time Remaining: <span id="remainingTime"></span></div>
        <div class="loader-container">
            <div class="loader">
                <span></span>
                <div id="WASHSTATUS" class="loader-text"></div>
            </div>
        </div>
        <div class="navtitle">
            <div style="border-radius: 10px !important;"></div>
            <button type="button" class="btn btn-power" id="btn0" onclick="ButtonPress0()"></button>
            <button type="button" class="btn btn-startstop" id="btn1" onclick="ButtonPress1()"></button>
        </div>
    </main>
    <div class="program-selector" id="programSelector">
        <div class="carousel"
            data-flickity='{ "cellAlign": "center", "contain": true, "wrapAround": true, "autoPlay": false, "prevNextButtons": false, "pageDots": false }'>
            <div class="carousel-cell" onclick="UpdateSlider(0)">COTTON/NORMAL</div>
            <div class="carousel-cell" onclick="UpdateSlider(1)">HEAVY DUTY</div>
            <div class="carousel-cell" onclick="UpdateSlider(2)">BULKY/LARGE</div>
            <div class="carousel-cell" onclick="UpdateSlider(3)">BRIGHT WHITES</div>
            <div class="carousel-cell" onclick="UpdateSlider(4)">SANITARY</div>
            <div class="carousel-cell" onclick="UpdateSlider(5)">ALLERGIENE</div>
            <div class="carousel-cell" onclick="UpdateSlider(6)">TUB CLEAN</div>
            <div class="carousel-cell" onclick="UpdateSlider(7)">JUMBO WASH</div>
            <div class="carousel-cell" onclick="UpdateSlider(8)">TOWELS</div>
            <div class="carousel-cell" onclick="UpdateSlider(9)">PERM. PRESS</div>
            <div class="carousel-cell" onclick="UpdateSlider(10)">HAND WASH/WOOL</div>
            <div class="carousel-cell" onclick="UpdateSlider(11)">DELICATES</div>
            <div class="carousel-cell" onclick="UpdateSlider(12)">SPEED WASH</div>
            <div class="carousel-cell" onclick="UpdateSlider(13)">SMALL LOAD</div>
        </div>
    </div>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/flickity/2.2.2/flickity.pkgd.min.js"></script>
    <script>
        var xmlHttp = createXmlHttpObject();
        function createXmlHttpObject() {
            if (window.XMLHttpRequest) {
                xmlHttp = new XMLHttpRequest();
            } else {
                xmlHttp = new ActiveXObject("Microsoft.XMLHTTP");
            }
            return xmlHttp;
        }
        function ButtonPress0() {
            var xhttp = new XMLHttpRequest();
            xhttp.open("PUT", "BUTTON_0", false);
            xhttp.send();
        }
        function ButtonPress1() {
            var xhttp = new XMLHttpRequest();
            xhttp.open("PUT", "BUTTON_1", false);
            xhttp.send();
        }
        function UpdateSlider(value) {
            var xhttp = new XMLHttpRequest();
            xhttp.onreadystatechange = function () {
                if (this.readyState == 4 && this.status == 200) {
                    document.getElementById("fanrpm").innerHTML = this.responseText;
                }
            }
            xhttp.open("PUT", "UPDATE_SLIDER?VALUE=" + value, true);
            xhttp.send();
        }
        function response() {
            var message;
            var xmlResponse;
            var xmldoc;
            var dt = new Date();
            xmlResponse = xmlHttp.responseXML;
            xmldoc = xmlResponse.getElementsByTagName("ETA");
            message = xmldoc[0].firstChild.nodeValue;
            document.getElementById("remainingTime").innerHTML = message;
            xmldoc = xmlResponse.getElementsByTagName("WS");
            message = xmldoc[0].firstChild.nodeValue;
            document.getElementById("WASHSTATUS").innerHTML = message;
            xmldoc = xmlResponse.getElementsByTagName("LED");
            message = xmldoc[0].firstChild.nodeValue;
            if (message == 0) {
                document.getElementById("btn0").style.color = "red";
                document.getElementById("programSelector").style.display = "none"; 
            } else {
                document.getElementById("btn0").style.color = "green";
                document.getElementById("programSelector").style.display = "block";
            }

            xmldoc = xmlResponse.getElementsByTagName("SWITCH");
            message = xmldoc[0].firstChild.nodeValue;
        }
        function process() {
            if (xmlHttp.readyState == 0 || xmlHttp.readyState == 4) {
                xmlHttp.open("PUT", "xml", true);
                xmlHttp.onreadystatechange = response;
                xmlHttp.send(null);
            }
            setTimeout("process()", 500);
        }
        document.addEventListener('DOMContentLoaded', function () {
            var flkty = new Flickity('.carousel');
            flkty.on('change', function (index) {
                UpdateSlider(index);
            });
        });
    </script>
</body>
</html>
)=====";
