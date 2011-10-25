/**
 * KineticJS 2d JavaScript Library v1.0.3
 * http://www.kineticjs.com/
 * Copyright 2011, Eric Rowell
 * Licensed under the MIT or GPL Version 2 licenses.
 * Date: September 2 2011
 *
 * Copyright (C) 2011 by Eric Rowell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
var Kinetic_2d = function(canvasId) {
    this.canvas = document.getElementById(canvasId);
    this.context = this.canvas.getContext("2d");
    this.drawStage = undefined;
    this.listening = false;

    // desktop flags
    this.mousePos = null;
    this._mouseDown = false;
    this._mouseUp = false;
    this._mouseOver = false;
    this._mouseMove = false;

    // mobile flags
    this.touchPos = null;
    this._touchStart = false;
    this._touchMove = false;
    this._touchEnd = false;

    // Region Events
    this._currentRegion = null;
    this._regionIndex = 0;
    this._lastRegionIndex = -1;
    this._mouseOverRegionIndex = -1;

    // Animation
    this.t = 0;
    this.timeInterval = 0;
    this.startTime = 0;
    this.lastTime = 0;
    this.frame = 0;
    this.animating = false;
    
    this.mouseDown = false;

    // provided by Paul Irish
    window.requestAnimFrame = (function(callback) {
        return window.requestAnimationFrame || window.webkitRequestAnimationFrame || window.mozRequestAnimationFrame || window.oRequestAnimationFrame || window.msRequestAnimationFrame ||
        function(callback) {
            window.setTimeout(callback, 1000 / 40);
        };

    })();
};

// ======================================= GENERAL
// =======================================

Kinetic_2d.prototype.getContext = function() {
    return this.context;
};

Kinetic_2d.prototype.getCanvas = function() {
    return this.canvas;
};

Kinetic_2d.prototype.clear = function() {
    this.context.clearRect(0, 0, this.canvas.width, this.canvas.height);
};

Kinetic_2d.prototype.getCanvasPos = function() {
    var obj = this.getCanvas();
    var top = 0;
    var left = 0;
    while(obj.tagName != "BODY") {
        top += obj.offsetTop;
        left += obj.offsetLeft;
        obj = obj.offsetParent;
    }
    return {
        top : top,
        left : left
    };
};

Kinetic_2d.prototype._drawStage = function() {
    if(this.drawStage !== undefined) {
        this.drawStage();

        // desktop flags
        this._mouseOver = false;
        this._mouseMove = false;
        this._mouseDown = false;
        this._mouseUp = false;

        // mobile touch flags
        this._touchStart = false;
        this._touchMove = false;
        this._touchEnd = false;
    }
};

Kinetic_2d.prototype.setDrawStage = function(func) {
    this.drawStage = func;
    this.listen();
};

// ======================================= CANVAS EVENTS

Kinetic_2d.prototype.handleEvent = function(evt) {
    if(!evt) {
        evt = window.event;
    }

    this.setMousePosition(evt);
    this.setTouchPosition(evt);
    this._regionIndex = 0;

    if(!this.animating) {
        this._drawStage();
    }
};

Kinetic_2d.prototype.listen = function() {
    var that = this;
    this._drawStage();

    // desktop events
    this.canvas.addEventListener("mousedown", function(evt) {
        that._mouseDown = true;
        that.mouseDown = true;
        that.handleEvent(evt);
    }, false);

    this.canvas.addEventListener("mousemove", function(evt) {
        that.handleEvent(evt);
    }, false);

    this.canvas.addEventListener("mouseup", function(evt) {
        that._mouseUp = true;
        that.mouseDown = false;
        that.handleEvent(evt);
    }, false);

    this.canvas.addEventListener("mouseover", function(evt) {
        that.handleEvent(evt);
    }, false);

    this.canvas.addEventListener("mouseout", function(evt) {
        that.mousePos = null;
    }, false);

    // mobile events
    this.canvas.addEventListener("touchstart", function(evt) {
        evt.preventDefault();
        that._touchStart = true;
        that.handleEvent(evt);
    }, false);

    this.canvas.addEventListener("touchmove", function(evt) {
        evt.preventDefault();
        that.handleEvent(evt);
    }, false);

    this.canvas.addEventListener("touchend", function(evt) {
        evt.preventDefault();
        that._touchEnd = true;
        that.handleEvent(evt);
    }, false);

};

Kinetic_2d.prototype.getMousePos = function(evt) {
    return this.mousePos;
};

Kinetic_2d.prototype.getTouchPos = function(evt) {
    return this.touchPos;
};

Kinetic_2d.prototype.setMousePosition = function(evt) {
    var mouseX = evt.clientX - this.getCanvasPos().left + window.pageXOffset;
    var mouseY = evt.clientY - this.getCanvasPos().top + window.pageYOffset;
    this.mousePos = {
        x : mouseX,
        y : mouseY
    };
};

Kinetic_2d.prototype.setTouchPosition = function(evt) {
    if(evt.touches !== undefined && evt.touches.length == 1) { // Only deal with
        // one finger
        var touch = evt.touches[0];
        // Get the information for finger #1
        var touchX = touch.pageX - this.getCanvasPos().left + window.pageXOffset;
        var touchY = touch.pageY - this.getCanvasPos().top + window.pageYOffset;

        this.touchPos = {
            x : touchX,
            y : touchY
        };
    }
};

Kinetic_2d.prototype.isMouseDown = function() {
    return this.mouseDown;  
};

// ======================================= REGION EVENTS

Kinetic_2d.prototype.beginRegion = function() {
    this._currentRegion = {};
    this._regionIndex++;
};

Kinetic_2d.prototype.addRegionEventListener = function(type, func) {
    var event = (type.indexOf('touch') == -1) ? 'on' + type : type;
    this._currentRegion[event] = func;
};

Kinetic_2d.prototype.closeRegion = function() {
    var pos = this.touchPos || this.mousePos;

    if(pos !== null && this.context.isPointInPath(pos.x, pos.y)) {
        if(this._lastRegionIndex != this._regionIndex) {
            this._lastRegionIndex = this._regionIndex;
        }

        // handle onmousedown
        if(this._mouseDown && this._currentRegion.onmousedown !== undefined) {
            this._currentRegion.onmousedown();
            this._mouseDown = false;
        }

        // handle onmouseup
        else if(this._mouseUp && this._currentRegion.onmouseup !== undefined) {
            this._currentRegion.onmouseup();
            this._mouseUp = false;
        }

        // handle onmouseover
        else if(!this._mouseOver && this._regionIndex != this._mouseOverRegionIndex && this._currentRegion.onmouseover !== undefined) {
            this._currentRegion.onmouseover();
            this._mouseOver = true;
            this._mouseOverRegionIndex = this._regionIndex;
        }

        // handle onmousemove
        else if(!this._mouseMove && this._currentRegion.onmousemove !== undefined) {
            this._currentRegion.onmousemove();
            this._mouseMove = true;
        }

        // handle touchstart
        if(this._touchStart && this._currentRegion._touchStart !== undefined) {
            this._currentRegion._touchStart();
            this._touchStart = false;
        }

        // handle touchend
        if(this._touchEnd && this._currentRegion._touchEnd !== undefined) {
            this._currentRegion._touchEnd();
            this._touchEnd = false;
        }

        // handle touchmove
        if(!this._touchMove && this._currentRegion._touchMove !== undefined) {
            this._currentRegion._touchMove();
            this._touchMove = true;
        }

    }
    else if(this._regionIndex == this._lastRegionIndex) {
        this._lastRegionIndex = -1;
        this._mouseOverRegionIndex = -1;

        // handle mouseout condition
        if(this._currentRegion.onmouseout !== undefined) {
            this._currentRegion.onmouseout();
        }
    }
};

// ======================================= ANIMATION
// =======================================

Kinetic_2d.prototype.isAnimating = function() {
    return this.animating;
};

Kinetic_2d.prototype.getFrame = function() {
    return this.frame;
};

Kinetic_2d.prototype.startAnimation = function() {
    this.animating = true;
    var date = new Date();
    this.startTime = date.getTime();
    this.lastTime = this.startTime;

    this._drawStage();

    this.animationLoop();
};

Kinetic_2d.prototype.stopAnimation = function() {
    this.animating = false;
};

Kinetic_2d.prototype.getTimeInterval = function() {
    return this.timeInterval;
};

Kinetic_2d.prototype.getTime = function() {
    return this.t;
};

Kinetic_2d.prototype.getFps = function() {
    return this.timeInterval > 0 ? 1000 / this.timeInterval : 0;
};

Kinetic_2d.prototype.animationLoop = function() {
    var that = this;

    this.frame++;
    var date = new Date();
    var thisTime = date.getTime();
    this.timeInterval = thisTime - this.lastTime;
    this.t += this.timeInterval;
    this.lastTime = thisTime;

    this._drawStage();

    if(this.animating) {
        requestAnimFrame(function() {
            that.animationLoop();
        });

    }
};
