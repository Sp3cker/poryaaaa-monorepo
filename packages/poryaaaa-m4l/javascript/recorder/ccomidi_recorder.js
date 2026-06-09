(() => {
  var __create = Object.create;
  var __defProp = Object.defineProperty;
  var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
  var __getOwnPropNames = Object.getOwnPropertyNames;
  var __getProtoOf = Object.getPrototypeOf;
  var __hasOwnProp = Object.prototype.hasOwnProperty;
  var __commonJS = (cb, mod) => function __require() {
    return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
  };
  var __copyProps = (to, from, except, desc) => {
    if (from && typeof from === "object" || typeof from === "function") {
      for (let key of __getOwnPropNames(from))
        if (!__hasOwnProp.call(to, key) && key !== except)
          __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
    }
    return to;
  };
  var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
    // If the importer is in node compatibility mode or this is not an ESM
    // file that has been converted to a CommonJS file using a Babel-
    // compatible transform (i.e. "__esModule" has not been set), then set
    // "default" to the CommonJS "module.exports" for node compatibility.
    isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
    mod
  ));

  // node_modules/midi-writer-js/build/index.js
  var require_build = __commonJS({
    "node_modules/midi-writer-js/build/index.js"(exports, module) {
      "use strict";
      var Constants = {
        VERSION: "3.2.1",
        HEADER_CHUNK_TYPE: [77, 84, 104, 100],
        HEADER_CHUNK_LENGTH: [0, 0, 0, 6],
        HEADER_CHUNK_FORMAT0: [0, 0],
        HEADER_CHUNK_FORMAT1: [0, 1],
        HEADER_CHUNK_DIVISION: [0, 128],
        TRACK_CHUNK_TYPE: [77, 84, 114, 107],
        META_EVENT_ID: 255,
        META_SMTPE_OFFSET: 84
      };
      var fillStr = (s, n) => Array(Math.abs(n) + 1).join(s);
      function isNamed(src) {
        return src !== null && typeof src === "object" && typeof src.name === "string" ? true : false;
      }
      function isPitch(pitch) {
        return pitch !== null && typeof pitch === "object" && typeof pitch.step === "number" && typeof pitch.alt === "number" ? true : false;
      }
      var FIFTHS = [0, 2, 4, -1, 1, 3, 5];
      var STEPS_TO_OCTS = FIFTHS.map(
        (fifths) => Math.floor(fifths * 7 / 12)
      );
      function encode(pitch) {
        const { step, alt, oct, dir = 1 } = pitch;
        const f = FIFTHS[step] + 7 * alt;
        if (oct === void 0) {
          return [dir * f];
        }
        const o = oct - STEPS_TO_OCTS[step] - 4 * alt;
        return [dir * f, dir * o];
      }
      var NoNote = { empty: true, name: "", pc: "", acc: "" };
      var cache = /* @__PURE__ */ new Map();
      var stepToLetter = (step) => "CDEFGAB".charAt(step);
      var altToAcc = (alt) => alt < 0 ? fillStr("b", -alt) : fillStr("#", alt);
      var accToAlt = (acc) => acc[0] === "b" ? -acc.length : acc.length;
      function note(src) {
        const stringSrc = JSON.stringify(src);
        const cached = cache.get(stringSrc);
        if (cached) {
          return cached;
        }
        const value = typeof src === "string" ? parse(src) : isPitch(src) ? note(pitchName(src)) : isNamed(src) ? note(src.name) : NoNote;
        cache.set(stringSrc, value);
        return value;
      }
      var REGEX = /^([a-gA-G]?)(#{1,}|b{1,}|x{1,}|)(-?\d*)\s*(.*)$/;
      function tokenizeNote(str) {
        const m = REGEX.exec(str);
        return [m[1].toUpperCase(), m[2].replace(/x/g, "##"), m[3], m[4]];
      }
      var mod = (n, m) => (n % m + m) % m;
      var SEMI = [0, 2, 4, 5, 7, 9, 11];
      function parse(noteName) {
        const tokens = tokenizeNote(noteName);
        if (tokens[0] === "" || tokens[3] !== "") {
          return NoNote;
        }
        const letter = tokens[0];
        const acc = tokens[1];
        const octStr = tokens[2];
        const step = (letter.charCodeAt(0) + 3) % 7;
        const alt = accToAlt(acc);
        const oct = octStr.length ? +octStr : void 0;
        const coord = encode({ step, alt, oct });
        const name = letter + acc + octStr;
        const pc = letter + acc;
        const chroma = (SEMI[step] + alt + 120) % 12;
        const height = oct === void 0 ? mod(SEMI[step] + alt, 12) - 12 * 99 : SEMI[step] + alt + 12 * (oct + 1);
        const midi = height >= 0 && height <= 127 ? height : null;
        const freq = oct === void 0 ? null : Math.pow(2, (height - 69) / 12) * 440;
        return {
          empty: false,
          acc,
          alt,
          chroma,
          coord,
          freq,
          height,
          letter,
          midi,
          name,
          oct,
          pc,
          step
        };
      }
      function pitchName(props) {
        const { step, alt, oct } = props;
        const letter = stepToLetter(step);
        if (!letter) {
          return "";
        }
        const pc = letter + altToAcc(alt);
        return oct || oct === 0 ? pc + oct : pc;
      }
      function isMidi(arg) {
        return +arg >= 0 && +arg <= 127;
      }
      function toMidi(note$1) {
        if (isMidi(note$1)) {
          return +note$1;
        }
        const n = note(note$1);
        return n.empty ? null : n.midi;
      }
      var Utils = (
        /** @class */
        function() {
          function Utils2() {
          }
          Utils2.version = function() {
            return "3.2.0";
          };
          Utils2.stringToBytes = function(string) {
            return string.split("").map(function(char) {
              return char.charCodeAt(0);
            });
          };
          Utils2.isNumeric = function(n) {
            return !isNaN(parseFloat(n)) && isFinite(n);
          };
          Utils2.getPitch = function(pitch, middleC) {
            if (middleC === void 0) {
              middleC = "C4";
            }
            return 60 - toMidi(middleC) + toMidi(pitch);
          };
          Utils2.numberToVariableLength = function(ticks) {
            ticks = Math.round(ticks);
            var buffer = ticks & 127;
            while (ticks = ticks >> 7) {
              buffer <<= 8;
              buffer |= ticks & 127 | 128;
            }
            var bList = [];
            while (true) {
              bList.push(buffer & 255);
              if (buffer & 128)
                buffer >>= 8;
              else {
                break;
              }
            }
            return bList;
          };
          Utils2.stringByteCount = function(s) {
            return encodeURI(s).split(/%..|./).length - 1;
          };
          Utils2.numberFromBytes = function(bytes) {
            var hex = "";
            var stringResult;
            bytes.forEach(function(byte) {
              stringResult = byte.toString(16);
              if (stringResult.length == 1)
                stringResult = "0" + stringResult;
              hex += stringResult;
            });
            return parseInt(hex, 16);
          };
          Utils2.numberToBytes = function(number, bytesNeeded) {
            bytesNeeded = bytesNeeded || 1;
            var hexString = number.toString(16);
            if (hexString.length & 1) {
              hexString = "0" + hexString;
            }
            var hexArray = hexString.match(/.{2}/g);
            var intArray = hexArray.map(function(item) {
              return parseInt(item, 16);
            });
            if (intArray.length < bytesNeeded) {
              while (bytesNeeded - intArray.length > 0) {
                intArray.unshift(0);
              }
            }
            return intArray;
          };
          Utils2.toArray = function(value) {
            if (Array.isArray(value))
              return value;
            return [value];
          };
          Utils2.convertVelocity = function(velocity) {
            velocity = velocity > 100 ? 100 : velocity;
            return Math.round(velocity / 100 * 127);
          };
          Utils2.getTickDuration = function(duration, ticksPerBeat) {
            if (ticksPerBeat === void 0) {
              ticksPerBeat = 128;
            }
            if (Array.isArray(duration)) {
              return duration.map(function(value) {
                return Utils2.getTickDuration(value, ticksPerBeat);
              }).reduce(function(a, b) {
                return a + b;
              }, 0);
            }
            duration = duration.toString();
            if (duration.toLowerCase().charAt(0) === "t") {
              var ticks = parseInt(duration.substring(1));
              if (isNaN(ticks) || ticks < 0) {
                throw new Error(duration + " is not a valid duration.");
              }
              return ticks;
            }
            var tickDuration = ticksPerBeat * Utils2.getDurationMultiplier(duration);
            return Utils2.getRoundedIfClose(tickDuration);
          };
          Utils2.getRoundedIfClose = function(tick) {
            var roundedTick = Math.round(tick);
            return Math.abs(roundedTick - tick) < 1e-6 ? roundedTick : tick;
          };
          Utils2.getPrecisionLoss = function(tick) {
            var roundedTick = Math.round(tick);
            return roundedTick - tick;
          };
          Utils2.getDurationMultiplier = function(duration) {
            if (duration === "0")
              return 0;
            var match = duration.match(/^(?<dotted>d+)?(?<base>\d+)(?:t(?<tuplet>\d*))?/);
            if (match) {
              var base = Number(match.groups.base);
              var isValidBase = base === 1 || (base & base - 1) === 0;
              if (isValidBase) {
                var ratio = base / 4;
                var durationInQuarters = 1 / ratio;
                var _a = match.groups, dotted = _a.dotted, tuplet = _a.tuplet;
                if (dotted) {
                  var thisManyDots = dotted.length;
                  var divisor = Math.pow(2, thisManyDots);
                  durationInQuarters = durationInQuarters + durationInQuarters * ((divisor - 1) / divisor);
                }
                if (typeof tuplet === "string") {
                  var fitInto = durationInQuarters * 2;
                  var thisManyNotes = Number(tuplet || "3");
                  durationInQuarters = fitInto / thisManyNotes;
                }
                return durationInQuarters;
              }
            }
            throw new Error(duration + " is not a valid duration.");
          };
          return Utils2;
        }()
      );
      var ControllerChangeEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function ControllerChangeEvent2(fields) {
            this.channel = fields.channel - 1 || 0;
            this.controllerValue = fields.controllerValue;
            this.controllerNumber = fields.controllerNumber;
            this.delta = fields.delta || 0;
            this.name = "ControllerChangeEvent";
            this.status = 176;
            this.data = Utils.numberToVariableLength(fields.delta).concat(this.status | this.channel, this.controllerNumber, this.controllerValue);
          }
          return ControllerChangeEvent2;
        }()
      );
      var CopyrightEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function CopyrightEvent2(fields) {
            this.delta = fields.delta || 0;
            this.name = "CopyrightEvent";
            this.text = fields.text;
            this.type = 2;
            var textBytes = Utils.stringToBytes(this.text);
            this.data = Utils.numberToVariableLength(this.delta).concat(
              Constants.META_EVENT_ID,
              this.type,
              Utils.numberToVariableLength(textBytes.length),
              // Size
              textBytes
            );
          }
          return CopyrightEvent2;
        }()
      );
      var CuePointEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function CuePointEvent2(fields) {
            this.delta = fields.delta || 0;
            this.name = "CuePointEvent";
            this.text = fields.text;
            this.type = 7;
            var textBytes = Utils.stringToBytes(this.text);
            this.data = Utils.numberToVariableLength(this.delta).concat(
              Constants.META_EVENT_ID,
              this.type,
              Utils.numberToVariableLength(textBytes.length),
              // Size
              textBytes
            );
          }
          return CuePointEvent2;
        }()
      );
      var EndTrackEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function EndTrackEvent2(fields) {
            this.delta = (fields === null || fields === void 0 ? void 0 : fields.delta) || 0;
            this.name = "EndTrackEvent";
            this.type = [47, 0];
            this.data = Utils.numberToVariableLength(this.delta).concat(Constants.META_EVENT_ID, this.type);
          }
          return EndTrackEvent2;
        }()
      );
      var InstrumentNameEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function InstrumentNameEvent2(fields) {
            this.delta = fields.delta || 0;
            this.name = "InstrumentNameEvent";
            this.text = fields.text;
            this.type = 4;
            var textBytes = Utils.stringToBytes(this.text);
            this.data = Utils.numberToVariableLength(this.delta).concat(
              Constants.META_EVENT_ID,
              this.type,
              Utils.numberToVariableLength(textBytes.length),
              // Size
              textBytes
            );
          }
          return InstrumentNameEvent2;
        }()
      );
      var KeySignatureEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function KeySignatureEvent2(sf, mi) {
            this.name = "KeySignatureEvent";
            this.type = 89;
            var mode = mi || 0;
            sf = sf || 0;
            if (typeof mi === "undefined") {
              var fifths = [
                ["Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#"],
                ["ab", "eb", "bb", "f", "c", "g", "d", "a", "e", "b", "f#", "c#", "g#", "d#", "a#"]
              ];
              var _sflen = sf.length;
              var note2 = sf || "C";
              if (sf[0] === sf[0].toLowerCase())
                mode = 1;
              if (_sflen > 1) {
                switch (sf.charAt(_sflen - 1)) {
                  case "m":
                    mode = 1;
                    note2 = sf.charAt(0).toLowerCase();
                    note2 = note2.concat(sf.substring(1, _sflen - 1));
                    break;
                  case "-":
                    mode = 1;
                    note2 = sf.charAt(0).toLowerCase();
                    note2 = note2.concat(sf.substring(1, _sflen - 1));
                    break;
                  case "M":
                    mode = 0;
                    note2 = sf.charAt(0).toUpperCase();
                    note2 = note2.concat(sf.substring(1, _sflen - 1));
                    break;
                  case "+":
                    mode = 0;
                    note2 = sf.charAt(0).toUpperCase();
                    note2 = note2.concat(sf.substring(1, _sflen - 1));
                    break;
                }
              }
              var fifthindex = fifths[mode].indexOf(note2);
              sf = fifthindex === -1 ? 0 : fifthindex - 7;
            }
            this.data = Utils.numberToVariableLength(0).concat(
              Constants.META_EVENT_ID,
              this.type,
              [2],
              // Size
              Utils.numberToBytes(sf, 1),
              // Number of sharp or flats ( < 0 flat; > 0 sharp)
              Utils.numberToBytes(mode, 1)
            );
          }
          return KeySignatureEvent2;
        }()
      );
      var LyricEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function LyricEvent2(fields) {
            this.delta = fields.delta || 0;
            this.name = "LyricEvent";
            this.text = fields.text;
            this.type = 5;
            var textBytes = Utils.stringToBytes(this.text);
            this.data = Utils.numberToVariableLength(this.delta).concat(
              Constants.META_EVENT_ID,
              this.type,
              Utils.numberToVariableLength(textBytes.length),
              // Size
              textBytes
            );
          }
          return LyricEvent2;
        }()
      );
      var MarkerEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function MarkerEvent2(fields) {
            this.delta = fields.delta || 0;
            this.name = "MarkerEvent";
            this.text = fields.text;
            this.type = 6;
            var textBytes = Utils.stringToBytes(this.text);
            this.data = Utils.numberToVariableLength(this.delta).concat(
              Constants.META_EVENT_ID,
              this.type,
              Utils.numberToVariableLength(textBytes.length),
              // Size
              textBytes
            );
          }
          return MarkerEvent2;
        }()
      );
      var NoteOnEvent = (
        /** @class */
        function() {
          function NoteOnEvent2(fields) {
            this.name = "NoteOnEvent";
            this.channel = fields.channel || 1;
            this.pitch = fields.pitch;
            this.wait = fields.wait || 0;
            this.velocity = fields.velocity || 50;
            this.tick = fields.tick || null;
            this.delta = null;
            this.data = fields.data;
            this.status = 144;
          }
          NoteOnEvent2.prototype.buildData = function(track, precisionDelta, options) {
            if (options === void 0) {
              options = {};
            }
            this.data = [];
            var ticksPerBeat = options.ticksPerBeat || 128;
            if (this.tick) {
              this.tick = Utils.getRoundedIfClose(this.tick);
              if (track.tickPointer == 0) {
                this.delta = this.tick;
              }
            } else {
              this.delta = Utils.getTickDuration(this.wait, ticksPerBeat);
              this.tick = Utils.getRoundedIfClose(track.tickPointer + this.delta);
            }
            this.deltaWithPrecisionCorrection = Utils.getRoundedIfClose(this.delta - precisionDelta);
            this.data = Utils.numberToVariableLength(this.deltaWithPrecisionCorrection).concat(this.status | this.channel - 1, Utils.getPitch(this.pitch, options.middleC), Utils.convertVelocity(this.velocity));
            return this;
          };
          return NoteOnEvent2;
        }()
      );
      var NoteOffEvent = (
        /** @class */
        function() {
          function NoteOffEvent2(fields) {
            this.name = "NoteOffEvent";
            this.channel = fields.channel || 1;
            this.pitch = fields.pitch;
            this.velocity = fields.velocity || 50;
            this.tick = fields.tick || null;
            this.data = fields.data;
            this.delta = fields.delta !== void 0 ? fields.delta : Utils.getTickDuration(fields.duration);
            this.status = 128;
          }
          NoteOffEvent2.prototype.buildData = function(track, precisionDelta, options) {
            if (options === void 0) {
              options = {};
            }
            if (this.tick === null) {
              this.tick = Utils.getRoundedIfClose(this.delta + track.tickPointer);
            }
            this.deltaWithPrecisionCorrection = Utils.getRoundedIfClose(this.delta - precisionDelta);
            this.data = Utils.numberToVariableLength(this.deltaWithPrecisionCorrection).concat(this.status | this.channel - 1, Utils.getPitch(this.pitch, options.middleC), Utils.convertVelocity(this.velocity));
            return this;
          };
          return NoteOffEvent2;
        }()
      );
      var NoteEvent = (
        /** @class */
        function() {
          function NoteEvent2(fields) {
            this.data = [];
            this.name = "NoteEvent";
            this.pitch = Utils.toArray(fields.pitch);
            this.channel = fields.channel || 1;
            this.duration = fields.duration || "4";
            this.grace = fields.grace;
            this.repeat = fields.repeat || 1;
            this.sequential = fields.sequential || false;
            this.tick = fields.startTick || fields.tick || null;
            this.velocity = fields.velocity || 50;
            this.wait = fields.wait || 0;
            this.tickDuration = Utils.getTickDuration(this.duration);
            this.restDuration = Utils.getTickDuration(this.wait);
            this.events = [];
          }
          NoteEvent2.prototype.buildData = function(options) {
            var _this = this;
            if (options === void 0) {
              options = {};
            }
            this.data = [];
            this.events = [];
            var ticksPerBeat = options.ticksPerBeat || 128;
            if (this.grace) {
              var graceDuration_1 = 1;
              this.grace = Utils.toArray(this.grace);
              this.grace.forEach(function() {
                var _a;
                var noteEvent = new NoteEvent2({ pitch: _this.grace, duration: "T" + graceDuration_1 });
                (_a = _this.data).push.apply(_a, noteEvent.data);
              });
            }
            if (!this.sequential) {
              for (var j = 0; j < this.repeat; j++) {
                this.pitch.forEach(function(p, i) {
                  var noteOnNew;
                  if (i == 0) {
                    noteOnNew = new NoteOnEvent({
                      channel: _this.channel,
                      wait: _this.wait,
                      delta: Utils.getTickDuration(_this.wait, ticksPerBeat),
                      velocity: _this.velocity,
                      pitch: p,
                      tick: _this.tick
                    });
                  } else {
                    noteOnNew = new NoteOnEvent({
                      channel: _this.channel,
                      wait: 0,
                      delta: 0,
                      velocity: _this.velocity,
                      pitch: p,
                      tick: _this.tick
                    });
                  }
                  _this.events.push(noteOnNew);
                });
                this.pitch.forEach(function(p, i) {
                  var noteOffNew;
                  if (i == 0) {
                    noteOffNew = new NoteOffEvent({
                      channel: _this.channel,
                      duration: _this.duration,
                      delta: Utils.getTickDuration(_this.duration, ticksPerBeat),
                      velocity: _this.velocity,
                      pitch: p,
                      tick: _this.tick !== null ? Utils.getTickDuration(_this.duration, ticksPerBeat) + _this.tick : null
                    });
                  } else {
                    noteOffNew = new NoteOffEvent({
                      channel: _this.channel,
                      duration: 0,
                      delta: 0,
                      velocity: _this.velocity,
                      pitch: p,
                      tick: _this.tick !== null ? Utils.getTickDuration(_this.duration, ticksPerBeat) + _this.tick : null
                    });
                  }
                  _this.events.push(noteOffNew);
                });
              }
            } else {
              for (var j = 0; j < this.repeat; j++) {
                this.pitch.forEach(function(p, i) {
                  var noteOnNew = new NoteOnEvent({
                    channel: _this.channel,
                    wait: i > 0 ? 0 : _this.wait,
                    delta: i > 0 ? 0 : Utils.getTickDuration(_this.wait, ticksPerBeat),
                    velocity: _this.velocity,
                    pitch: p,
                    tick: _this.tick
                  });
                  var noteOffNew = new NoteOffEvent({
                    channel: _this.channel,
                    duration: _this.duration,
                    delta: Utils.getTickDuration(_this.duration, ticksPerBeat),
                    velocity: _this.velocity,
                    pitch: p
                  });
                  _this.events.push(noteOnNew, noteOffNew);
                });
              }
            }
            return this;
          };
          return NoteEvent2;
        }()
      );
      var PitchBendEvent = (
        /** @class */
        function() {
          function PitchBendEvent2(fields) {
            this.channel = fields.channel || 0;
            this.delta = fields.delta || 0;
            this.name = "PitchBendEvent";
            this.status = 224;
            var bend14 = this.scale14bits(fields.bend);
            var lsbValue = bend14 & 127;
            var msbValue = bend14 >> 7 & 127;
            this.data = Utils.numberToVariableLength(this.delta).concat(this.status | this.channel, lsbValue, msbValue);
          }
          PitchBendEvent2.prototype.scale14bits = function(zeroOne) {
            if (zeroOne <= 0) {
              return Math.floor(16384 * (zeroOne + 1) / 2);
            }
            return Math.floor(16383 * (zeroOne + 1) / 2);
          };
          return PitchBendEvent2;
        }()
      );
      var ProgramChangeEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function ProgramChangeEvent2(fields) {
            this.channel = fields.channel || 0;
            this.delta = fields.delta || 0;
            this.instrument = fields.instrument;
            this.status = 192;
            this.name = "ProgramChangeEvent";
            this.data = Utils.numberToVariableLength(this.delta).concat(this.status | this.channel, this.instrument);
          }
          return ProgramChangeEvent2;
        }()
      );
      var TempoEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function TempoEvent2(fields) {
            this.bpm = fields.bpm;
            this.delta = fields.delta || 0;
            this.tick = fields.tick;
            this.name = "TempoEvent";
            this.type = 81;
            var tempo = Math.round(6e7 / this.bpm);
            this.data = Utils.numberToVariableLength(this.delta).concat(
              Constants.META_EVENT_ID,
              this.type,
              [3],
              // Size
              Utils.numberToBytes(tempo, 3)
            );
          }
          return TempoEvent2;
        }()
      );
      var TextEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function TextEvent2(fields) {
            this.delta = fields.delta || 0;
            this.text = fields.text;
            this.name = "TextEvent";
            this.type = 1;
            var textBytes = Utils.stringToBytes(this.text);
            this.data = Utils.numberToVariableLength(fields.delta).concat(
              Constants.META_EVENT_ID,
              this.type,
              Utils.numberToVariableLength(textBytes.length),
              // Size
              textBytes
            );
          }
          return TextEvent2;
        }()
      );
      var TimeSignatureEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function TimeSignatureEvent2(numerator, denominator, midiclockspertick, notespermidiclock) {
            this.name = "TimeSignatureEvent";
            this.type = 88;
            this.data = Utils.numberToVariableLength(0).concat(
              Constants.META_EVENT_ID,
              this.type,
              [4],
              // Size
              Utils.numberToBytes(numerator, 1),
              // Numerator, 1 bytes
              Utils.numberToBytes(Math.log2(denominator), 1),
              // Denominator is expressed as pow of 2, 1 bytes
              Utils.numberToBytes(midiclockspertick || 24, 1),
              // MIDI Clocks per tick, 1 bytes
              Utils.numberToBytes(notespermidiclock || 8, 1)
            );
          }
          return TimeSignatureEvent2;
        }()
      );
      var TrackNameEvent = (
        /** @class */
        /* @__PURE__ */ function() {
          function TrackNameEvent2(fields) {
            this.delta = fields.delta || 0;
            this.name = "TrackNameEvent";
            this.text = fields.text;
            this.type = 3;
            var textBytes = Utils.stringToBytes(this.text);
            this.data = Utils.numberToVariableLength(this.delta).concat(
              Constants.META_EVENT_ID,
              this.type,
              Utils.numberToVariableLength(textBytes.length),
              // Size
              textBytes
            );
          }
          return TrackNameEvent2;
        }()
      );
      var Track = (
        /** @class */
        function() {
          function Track2() {
            this.type = Constants.TRACK_CHUNK_TYPE;
            this.data = [];
            this.size = [];
            this.events = [];
            this.explicitTickEvents = [];
            this.tickPointer = 0;
          }
          Track2.prototype.addEvent = function(events, mapFunction) {
            var _this = this;
            Utils.toArray(events).forEach(function(event, i) {
              if (event instanceof NoteEvent) {
                if (typeof mapFunction === "function") {
                  var properties = mapFunction(i, event);
                  if (typeof properties === "object") {
                    Object.assign(event, properties);
                  }
                }
                if (event.tick !== null) {
                  _this.explicitTickEvents.push(event);
                } else {
                  _this.events.push(event);
                }
              } else {
                _this.events.push(event);
              }
            });
            return this;
          };
          Track2.prototype.buildData = function(options) {
            var _a;
            var _this = this;
            if (options === void 0) {
              options = {};
            }
            this.data = [];
            this.size = [];
            this.tickPointer = 0;
            var expandedEvents = [];
            this.events.forEach(function(event) {
              if (event instanceof NoteEvent && event.tick === null) {
                event.buildData(options).events.forEach(function(e) {
                  return expandedEvents.push(e);
                });
              } else {
                expandedEvents.push(event);
              }
            });
            this.events = expandedEvents;
            var precisionLoss = 0;
            this.events.forEach(function(event) {
              var _a2, _b, _c;
              if (event instanceof NoteOnEvent || event instanceof NoteOffEvent) {
                var built = event.buildData(_this, precisionLoss, options);
                precisionLoss = Utils.getPrecisionLoss(event.deltaWithPrecisionCorrection || 0);
                (_a2 = _this.data).push.apply(_a2, built.data);
                _this.tickPointer = Utils.getRoundedIfClose(event.tick);
              } else if (event instanceof TempoEvent) {
                _this.tickPointer = Utils.getRoundedIfClose(event.tick);
                (_b = _this.data).push.apply(_b, event.data);
              } else {
                event.tick = _this.tickPointer;
                (_c = _this.data).push.apply(_c, event.data);
              }
            });
            this.mergeExplicitTickEvents(options);
            if (!this.events.length || !(this.events[this.events.length - 1] instanceof EndTrackEvent)) {
              (_a = this.data).push.apply(_a, new EndTrackEvent().data);
            }
            this.size = Utils.numberToBytes(this.data.length, 4);
            return this;
          };
          Track2.prototype.mergeExplicitTickEvents = function(options) {
            var _this = this;
            if (options === void 0) {
              options = {};
            }
            if (!this.explicitTickEvents.length)
              return;
            this.explicitTickEvents.sort(function(a, b) {
              return a.tick - b.tick;
            });
            this.explicitTickEvents.forEach(function(noteEvent) {
              noteEvent.buildData(options).events.forEach(function(e) {
                return e.buildData(_this, 0, options);
              });
              noteEvent.events.forEach(function(event) {
                return _this.mergeSingleEvent(event);
              });
            });
            this.explicitTickEvents = [];
            this.buildData(options);
          };
          Track2.prototype.mergeTrack = function(track) {
            var _this = this;
            this.buildData();
            track.buildData().events.forEach(function(event) {
              return _this.mergeSingleEvent(event);
            });
            return this;
          };
          Track2.prototype.mergeSingleEvent = function(event) {
            if (!this.events.length) {
              this.addEvent(event);
              return this;
            }
            var lastEventIndex;
            for (var i = 0; i < this.events.length; i++) {
              if (this.events[i].tick > event.tick)
                break;
              lastEventIndex = i;
            }
            if (lastEventIndex === void 0) {
              event.delta = event.tick;
              this.events.splice(0, 0, event);
              if (this.events.length > 1) {
                this.events[1].delta = this.events[1].tick - event.tick;
              }
            } else {
              var splicedEventIndex = lastEventIndex + 1;
              event.delta = event.tick - this.events[lastEventIndex].tick;
              this.events.splice(splicedEventIndex, 0, event);
              for (var i = splicedEventIndex + 1; i < this.events.length; i++) {
                this.events[i].delta = this.events[i].tick - this.events[i - 1].tick;
              }
            }
            return this;
          };
          Track2.prototype.removeEventsByName = function(eventName) {
            this.events = this.events.filter(function(event) {
              return event.name !== eventName;
            });
            return this;
          };
          Track2.prototype.setTempo = function(bpm, tick) {
            if (tick === void 0) {
              tick = 0;
            }
            return this.addEvent(new TempoEvent({ bpm, tick }));
          };
          Track2.prototype.setTimeSignature = function(numerator, denominator, midiclockspertick, notespermidiclock) {
            return this.addEvent(new TimeSignatureEvent(numerator, denominator, midiclockspertick, notespermidiclock));
          };
          Track2.prototype.setKeySignature = function(sf, mi) {
            return this.addEvent(new KeySignatureEvent(sf, mi));
          };
          Track2.prototype.addText = function(text) {
            return this.addEvent(new TextEvent({ text }));
          };
          Track2.prototype.addCopyright = function(text) {
            return this.addEvent(new CopyrightEvent({ text }));
          };
          Track2.prototype.addTrackName = function(text) {
            return this.addEvent(new TrackNameEvent({ text }));
          };
          Track2.prototype.addInstrumentName = function(text) {
            return this.addEvent(new InstrumentNameEvent({ text }));
          };
          Track2.prototype.addMarker = function(text) {
            return this.addEvent(new MarkerEvent({ text }));
          };
          Track2.prototype.addCuePoint = function(text) {
            return this.addEvent(new CuePointEvent({ text }));
          };
          Track2.prototype.addLyric = function(text) {
            return this.addEvent(new LyricEvent({ text }));
          };
          Track2.prototype.polyModeOn = function() {
            var event = new NoteOnEvent({ data: [0, 176, 126, 0] });
            return this.addEvent(event);
          };
          Track2.prototype.setPitchBend = function(bend) {
            return this.addEvent(new PitchBendEvent({ bend }));
          };
          Track2.prototype.controllerChange = function(number, value, channel, delta) {
            return this.addEvent(new ControllerChangeEvent({ controllerNumber: number, controllerValue: value, channel, delta }));
          };
          return Track2;
        }()
      );
      var VexFlow = (
        /** @class */
        function() {
          function VexFlow2() {
          }
          VexFlow2.prototype.trackFromVoice = function(voice, options) {
            var _this = this;
            if (options === void 0) {
              options = { addRenderedAccidentals: false };
            }
            var track = new Track();
            var wait = [];
            voice.tickables.forEach(function(tickable) {
              if (tickable.noteType === "n") {
                track.addEvent(new NoteEvent({
                  pitch: tickable.keys.map(function(pitch, index) {
                    return _this.convertPitch(pitch, index, tickable, options.addRenderedAccidentals);
                  }),
                  duration: _this.convertDuration(tickable),
                  wait
                }));
                wait = [];
              } else if (tickable.noteType === "r") {
                wait.push(_this.convertDuration(tickable));
              }
            });
            if (wait.length > 0) {
              track.addEvent(new NoteEvent({ pitch: "[c4]", duration: "0", wait, velocity: "0" }));
            }
            return track;
          };
          VexFlow2.prototype.convertPitch = function(pitch, index, note2, addRenderedAccidentals) {
            var _a;
            if (addRenderedAccidentals === void 0) {
              addRenderedAccidentals = false;
            }
            var pitchParts = pitch.split("/");
            var accidentals = pitchParts[0].substring(1).replace("n", "");
            if (addRenderedAccidentals) {
              (_a = note2.getAccidentals()) === null || _a === void 0 ? void 0 : _a.forEach(function(accidental) {
                if (accidental.index === index) {
                  if (accidental.type === "n") {
                    accidentals = "";
                  } else {
                    accidentals += accidental.type;
                  }
                }
              });
            }
            return pitchParts[0][0] + accidentals + pitchParts[1];
          };
          VexFlow2.prototype.convertDuration = function(note2) {
            var dots = this.countDots(note2);
            return "d".repeat(dots) + this.convertBaseDuration(note2.duration) + (note2.tuplet ? "t" + note2.tuplet.num_notes : "");
          };
          VexFlow2.prototype.countDots = function(note2) {
            if (typeof note2.getModifiersByType === "function") {
              return note2.getModifiersByType("Dot").length;
            }
            if (Array.isArray(note2.modifiers)) {
              return note2.modifiers.filter(function(m) {
                var _a, _b;
                return ((_a = m.getCategory) === null || _a === void 0 ? void 0 : _a.call(m)) === "dots" || ((_b = m.attrs) === null || _b === void 0 ? void 0 : _b.type) === "Dot";
              }).length;
            }
            return note2.dots || 0;
          };
          VexFlow2.prototype.convertBaseDuration = function(duration) {
            switch (duration) {
              case "w":
                return "1";
              case "h":
                return "2";
              case "q":
                return "4";
              default:
                return duration;
            }
          };
          return VexFlow2;
        }()
      );
      function __spreadArray(to, from, pack) {
        if (pack || arguments.length === 2) for (var i = 0, l = from.length, ar; i < l; i++) {
          if (ar || !(i in from)) {
            if (!ar) ar = Array.prototype.slice.call(from, 0, i);
            ar[i] = from[i];
          }
        }
        return to.concat(ar || Array.prototype.slice.call(from));
      }
      var Header = (
        /** @class */
        /* @__PURE__ */ function() {
          function Header2(numberOfTracks, ticksPerBeat) {
            if (ticksPerBeat === void 0) {
              ticksPerBeat = 128;
            }
            this.type = Constants.HEADER_CHUNK_TYPE;
            var trackType = numberOfTracks > 1 ? Constants.HEADER_CHUNK_FORMAT1 : Constants.HEADER_CHUNK_FORMAT0;
            this.data = trackType.concat(
              Utils.numberToBytes(numberOfTracks, 2),
              // two bytes long,
              Utils.numberToBytes(ticksPerBeat, 2)
            );
            this.size = [0, 0, 0, this.data.length];
          }
          return Header2;
        }()
      );
      var Writer = (
        /** @class */
        function() {
          function Writer2(tracks, options) {
            if (options === void 0) {
              options = {};
            }
            this.tracks = Utils.toArray(tracks);
            this.options = options;
          }
          Writer2.prototype.buildData = function() {
            var _this = this;
            var data = [];
            var ticksPerBeat = this.options["ticksPerBeat"] || 128;
            data.push(new Header(this.tracks.length, ticksPerBeat));
            this.tracks.forEach(function(track) {
              data.push(track.buildData(_this.options));
            });
            return data;
          };
          Writer2.prototype.buildFile = function() {
            var build = [];
            this.buildData().forEach(function(d) {
              return build.push.apply(build, __spreadArray(__spreadArray(__spreadArray([], d.type, false), d.size, false), d.data, false));
            });
            return new Uint8Array(build);
          };
          Writer2.prototype.base64 = function() {
            if (typeof btoa === "function") {
              var bytes = this.buildFile();
              var len = bytes.byteLength;
              var chars = new Array(len);
              for (var i = 0; i < len; i++) {
                chars[i] = String.fromCharCode(bytes[i]);
              }
              return btoa(chars.join(""));
            }
            return Buffer.from(this.buildFile()).toString("base64");
          };
          Writer2.prototype.dataUri = function() {
            return "data:audio/midi;base64," + this.base64();
          };
          Writer2.prototype.setOption = function(key, value) {
            this.options[key] = value;
            return this;
          };
          Writer2.prototype.stdout = function() {
            return process.stdout.write(Buffer.from(this.buildFile()));
          };
          return Writer2;
        }()
      );
      var main = {
        Constants,
        ControllerChangeEvent,
        CopyrightEvent,
        CuePointEvent,
        EndTrackEvent,
        InstrumentNameEvent,
        KeySignatureEvent,
        LyricEvent,
        MarkerEvent,
        NoteOnEvent,
        NoteOffEvent,
        NoteEvent,
        PitchBendEvent,
        ProgramChangeEvent,
        TempoEvent,
        TextEvent,
        TimeSignatureEvent,
        Track,
        TrackNameEvent,
        Utils,
        VexFlow,
        Writer
      };
      module.exports = main;
    }
  });

  // code-src/recorder/recorder_smf_writer.ts
  var import_midi_writer_js = __toESM(require_build());
  var PPQ = 96;
  function parseBarBeatSixteenth(text, sigNum, sigDen) {
    const t = text.trim();
    if (!t) return null;
    const parts = t.split(".").map((p) => parseInt(p.trim(), 10));
    if (parts.some((p) => !Number.isFinite(p) || p < 1)) return null;
    const bar = parts[0];
    const beat = parts.length > 1 ? parts[1] : 1;
    const sixteenth = parts.length > 2 ? parts[2] : 1;
    const quartersPerBeat = 4 / sigDen;
    return (bar - 1) * sigNum * quartersPerBeat + (beat - 1) * quartersPerBeat + (sixteenth - 1) / 4;
  }
  function parseBeatNumber(text) {
    const t = text.trim();
    if (!t) return null;
    const n = Number(t);
    if (!Number.isFinite(n) || n < 0) return null;
    return n;
  }
  function basename(p) {
    const i = p.lastIndexOf("/");
    return i >= 0 ? p.slice(i + 1) : p;
  }
  function errorLabel(e) {
    if (e && typeof e === "object" && "message" in e && typeof e.message === "string" && e.message.trim()) {
      return e.message.trim();
    }
    if (e && typeof e === "object" && "name" in e && typeof e.name === "string") {
      return e.name;
    }
    return "Error";
  }
  function readU32(bytes, i) {
    return (bytes[i] & 255) << 24 | (bytes[i + 1] & 255) << 16 | (bytes[i + 2] & 255) << 8 | bytes[i + 3] & 255;
  }
  function isRealNoteOn(status, velocity) {
    return (status & 240) === 144 && (velocity & 127) > 0;
  }
  function validateSmfInitialChannelState(bytes) {
    const byChannel = /* @__PURE__ */ new Map();
    let i = 0;
    function readVarInt(trackEnd) {
      let v = 0;
      while (i < trackEnd) {
        const b = bytes[i++] & 255;
        v = v << 7 | b & 127;
        if ((b & 128) === 0) return v;
      }
      throw new Error("unterminated SMF variable-length quantity");
    }
    function flagsFor(ch) {
      let flags = byChannel.get(ch);
      if (!flags) {
        flags = { hasNote: false, pc: false, volume: false, pan: false };
        byChannel.set(ch, flags);
      }
      return flags;
    }
    if (bytes.length < 14 || bytes[0] !== 77 || bytes[1] !== 84 || bytes[2] !== 104 || bytes[3] !== 100) {
      throw new Error("not an SMF: MThd missing");
    }
    i += 4;
    const headerLen = readU32(bytes, i);
    i += 4;
    i += headerLen;
    let trackIdx = 0;
    while (i < bytes.length) {
      if (bytes[i] !== 77 || bytes[i + 1] !== 84 || bytes[i + 2] !== 114 || bytes[i + 3] !== 107) {
        break;
      }
      i += 4;
      const trackLen = readU32(bytes, i);
      i += 4;
      const trackEnd = i + trackLen;
      let runningStatus = 0;
      let absTick = 0;
      while (i < trackEnd) {
        absTick += readVarInt(trackEnd);
        let status = bytes[i] & 255;
        if (status < 128) {
          if (runningStatus === 0) throw new Error("SMF running status without prior status");
          status = runningStatus;
        } else {
          i++;
          if (status < 240) runningStatus = status;
        }
        if (status === 255) {
          i++;
          const len = readVarInt(trackEnd);
          i += len;
          continue;
        }
        if (status === 240 || status === 247) {
          const len = readVarInt(trackEnd);
          i += len;
          continue;
        }
        const high = status & 240;
        const ch = status & 15;
        const d1 = bytes[i++] & 255;
        const d2 = high === 192 || high === 208 ? 0 : bytes[i++] & 255;
        if (trackIdx === 0) continue;
        const flags = flagsFor(ch);
        if (isRealNoteOn(status, d2)) flags.hasNote = true;
        if (absTick === 0) {
          if (high === 192) flags.pc = true;
          else if (high === 176 && d1 === 7) flags.volume = true;
          else if (high === 176 && d1 === 10) flags.pan = true;
        }
      }
      i = trackEnd;
      trackIdx++;
    }
    const missing = [];
    for (const [ch, flags] of [...byChannel.entries()].sort((a, b) => a[0] - b[0])) {
      if (!flags.hasNote) continue;
      const label = `ch${ch + 1}`;
      if (!flags.pc) missing.push(`${label} Program Change`);
      if (!flags.volume) missing.push(`${label} Volume CC7`);
      if (!flags.pan) missing.push(`${label} Pan CC10`);
    }
    return { ok: missing.length === 0, missing };
  }
  function createSmfWriter(deps) {
    const status = deps.status ?? (() => {
    });
    return {
      async save() {
        const outPath = deps.outputPath();
        if (!outPath) {
          deps.post("recorder: no output path set \u2014 type a filename and press Enter\n");
          status("FAILED: no filename");
          return false;
        }
        status("Saving...");
        let tempPath = "";
        try {
          const dump = await deps.requestBufferDump();
          tempPath = dump.path;
          const events = deps.readBufferFile(tempPath);
          deps.post(`recorder: dump path=${tempPath} count=${dump.count} parsed=${events.length}
`);
          const tempo = deps.liveApi.getTempo();
          const loop = deps.liveApi.getLoop();
          const timeSig = deps.liveApi.getTimeSig();
          const voicemap = deps.voicemap();
          if (voicemap.size === 0) {
            deps.post(`recorder: no synthetic voicemap PCs configured; relying on captured MIDI for PC state
`);
          } else {
            const entries = [];
            for (const [ch, prog] of voicemap) {
              entries.push(`ch${ch + 1}=prog${prog}`);
            }
            deps.post(`recorder: voicemap has ${voicemap.size} entries: ${entries.join(", ")}
`);
          }
          const initialCcs = deps.readInitialCcs?.() ?? [];
          if (initialCcs.length > 0) {
            deps.post(`recorder: injecting ${initialCcs.length} legacy initial CCs at tick 0
`);
          } else {
            deps.post(`recorder: no legacy initial CCs configured; relying on captured MIDI for CC state
`);
          }
          let parsedRange;
          const rangeStrs = deps.range();
          const startStr = rangeStrs.start.trim();
          const lenStr = rangeStrs.length.trim();
          const sb = startStr ? parseBeatNumber(startStr) : null;
          const lb = lenStr ? parseBeatNumber(lenStr) : null;
          if (startStr && sb === null) {
            deps.post(`recorder: invalid Start "${startStr}" \u2014 ignoring
`);
          }
          if (lenStr && lb === null) {
            deps.post(`recorder: invalid Length "${lenStr}" \u2014 ignoring
`);
          }
          if (sb !== null && lb !== null) {
            const startBeats = sb;
            parsedRange = {
              startBeats,
              endBeats: startBeats + lb
            };
            deps.post(`recorder: export range start=${parsedRange.startBeats} length=${lb} end=${parsedRange.endBeats}
`);
          } else if (startStr || lenStr) {
            deps.post("recorder: incomplete export range - saving whole buffer\n");
          }
          let parsedMarkers = null;
          const markerStrs = deps.markerRange();
          const markerStartStr = markerStrs.start.trim();
          const markerEndStr = markerStrs.end.trim();
          const mb = markerStartStr ? parseBarBeatSixteenth(markerStartStr, timeSig.num, timeSig.den) : null;
          const me = markerEndStr ? parseBarBeatSixteenth(markerEndStr, timeSig.num, timeSig.den) : null;
          if (markerStartStr && mb === null) {
            deps.post(`recorder: invalid Loop Start "${markerStartStr}" \u2014 no loop markers
`);
          }
          if (markerEndStr && me === null) {
            deps.post(`recorder: invalid Loop End "${markerEndStr}" \u2014 no loop markers
`);
          }
          if (mb !== null && me !== null) {
            parsedMarkers = { startBeats: mb, endBeats: me };
            deps.post(`recorder: loop markers start=${mb} end=${me}
`);
          } else if (markerStartStr || markerEndStr) {
            deps.post("recorder: incomplete loop markers - no loop markers\n");
          }
          deps.post(`recorder: Live loop ignored for SMF markers on=${loop.on ? 1 : 0} start=${loop.start} length=${loop.length}
`);
          const smfBytes = buildSmf({
            events,
            tempo,
            loop,
            timeSig,
            voicemap,
            range: parsedRange,
            loopMarkers: parsedMarkers,
            anchorMode: "firstNote",
            initialCcs
          });
          deps.post(`recorder: smf bytes=${smfBytes.length}
`);
          const initialState = validateSmfInitialChannelState(smfBytes);
          if (!initialState.ok) {
            const missing = initialState.missing.join(", ");
            deps.post(
              `recorder: refusing export; missing tick-0 channel state: ${missing}
`
            );
            status("FAILED: missing tick-0 MIDI");
            return false;
          }
          const ok = deps.writeSmf(outPath, smfBytes);
          if (!ok) {
            deps.post(`recorder: FAILED to write ${outPath}
`);
            status("FAILED: write error");
            return false;
          }
          deps.post(`recorder: wrote ${outPath} (${events.length} events)
`);
          status(`Saved: ${basename(outPath)} (${events.length} events)`);
          try {
            deps.clearBuffer?.();
          } catch (_) {
          }
          return true;
        } catch (e) {
          deps.post(`recorder: save threw: ${String(e)}
`);
          status(`FAILED: ${errorLabel(e)}`);
          return false;
        } finally {
          if (tempPath) {
            try {
              deps.unlink(tempPath);
            } catch (_) {
            }
          }
        }
      }
    };
  }
  function capturedEventTick(beats, anchor) {
    const tick = Math.round((beats - anchor) * PPQ);
    return tick > 0 ? tick + 1 : 0;
  }
  function loopStateFromMarkers(markers) {
    return {
      on: true,
      start: markers.startBeats,
      length: Math.max(0, markers.endBeats - markers.startBeats)
    };
  }
  var XCMD_SELECTOR_CC = 30;
  var XCMD_VALUE_CC = 29;
  var VOLUME_CC = 7;
  var PAN_CC = 10;
  function buildWriterFile(writer) {
    const chunks = writer.buildData();
    let total = 0;
    for (const chunk of chunks) {
      total += chunk.type.length + chunk.size.length + chunk.data.length;
    }
    const bytes = new Uint8Array(total);
    let offset = 0;
    for (const chunk of chunks) {
      for (const part of [chunk.type, chunk.size, chunk.data]) {
        bytes.set(part, offset);
        offset += part.length;
      }
    }
    return bytes;
  }
  function xcmdPairKey(first, second) {
    if (first.kind !== "cc" || second.kind !== "cc") return null;
    if (first.cn !== XCMD_SELECTOR_CC || second.cn !== XCMD_VALUE_CC) return null;
    if (first.tick !== second.tick) return null;
    return `${first.tick}|${first.cv}|${second.cv}`;
  }
  function markDuplicateXcmdPairs(arr, superseded) {
    const seen = /* @__PURE__ */ new Set();
    for (let i = 0; i < arr.length - 1; i++) {
      const key = xcmdPairKey(arr[i], arr[i + 1]);
      if (key === null) continue;
      if (seen.has(key)) {
        superseded.add(i);
        superseded.add(i + 1);
      } else {
        seen.add(key);
      }
      i++;
    }
  }
  function buildSmf(input) {
    const { events, tempo, loop, timeSig, voicemap, range } = input;
    const firstNote = input.anchorMode === "firstNote" ? events.find((e) => isRealNoteOn(e.status, e.d2)) : void 0;
    const anchor = range ? range.startBeats : firstNote?.beats ?? 0;
    const rangeEnd = range ? range.endBeats : Number.POSITIVE_INFINITY;
    const explicitMarkers = input.loopMarkers;
    const markerLoop = "loopMarkers" in input ? explicitMarkers === null || explicitMarkers === void 0 ? { on: false, start: 0, length: 0 } : loopStateFromMarkers(explicitMarkers) : range && Number.isFinite(range.endBeats) ? { on: true, start: range.startBeats, length: Math.max(0, range.endBeats - range.startBeats) } : loop.on && events.length > 0 ? { on: true, start: 0, length: loop.length } : loop;
    const legacyLoopStartReplay = !("loopMarkers" in input) && !range && loop.on && events.length > 0;
    const conductor = new import_midi_writer_js.default.Track();
    conductor.setTempo(tempo);
    conductor.setTimeSignature(timeSig.num, timeSig.den, 24, 8);
    if (markerLoop.on) {
      const openTickAbs = Math.round((markerLoop.start - anchor) * PPQ);
      const closeTickAbs = Math.round((markerLoop.start + markerLoop.length - anchor) * PPQ);
      const openDelta = Math.max(0, openTickAbs);
      const closeDelta = Math.max(0, closeTickAbs - Math.max(0, openTickAbs));
      conductor.addEvent(new import_midi_writer_js.default.MarkerEvent({
        text: "[",
        delta: openDelta
      }));
      conductor.addEvent(new import_midi_writer_js.default.MarkerEvent({
        text: "]",
        delta: closeDelta
      }));
    }
    const pending = /* @__PURE__ */ new Map();
    const loopStartPcValue = /* @__PURE__ */ new Map();
    const loopStartCcValue = /* @__PURE__ */ new Map();
    const firstCapturedPcValue = /* @__PURE__ */ new Map();
    const openNotes = /* @__PURE__ */ new Map();
    let insCounter = 0;
    const loopStartTick = markerLoop.on ? Math.max(0, Math.round((markerLoop.start - anchor) * PPQ)) : 0;
    function ensureChan(ch) {
      let arr = pending.get(ch);
      if (!arr) {
        arr = [];
        pending.set(ch, arr);
      }
      return arr;
    }
    function rememberLoopStartCc(ch, cc, value) {
      if (cc !== VOLUME_CC && cc !== PAN_CC) return;
      let byCc = loopStartCcValue.get(ch);
      if (!byCc) {
        byCc = /* @__PURE__ */ new Map();
        loopStartCcValue.set(ch, byCc);
      }
      byCc.set(cc, value);
    }
    for (const [ch, prog] of voicemap) {
      if (ch < 0 || ch > 15) continue;
      if (prog < 0) continue;
      ensureChan(ch).push({
        kind: "pc",
        channel: ch,
        program: prog,
        tick: 0,
        insOrder: insCounter++,
        synthetic: true
      });
      if (markerLoop.on) loopStartPcValue.set(ch, prog);
    }
    if (input.initialCcs) {
      for (const ic of input.initialCcs) {
        if (ic.channel < 0 || ic.channel > 15) continue;
        if (ic.cc < 0 || ic.cc > 127) continue;
        if (ic.value < 0 || ic.value > 127) continue;
        ensureChan(ic.channel).push({
          kind: "cc",
          channel: ic.channel,
          cn: ic.cc,
          cv: ic.value,
          tick: 0,
          insOrder: insCounter++
        });
        if (markerLoop.on) rememberLoopStartCc(ic.channel, ic.cc, ic.value);
      }
    }
    let lastEventTick = 0;
    for (const e of events) {
      const type = e.status >> 4 & 15;
      const ch = e.status & 15;
      const d1 = e.d1 & 127;
      const d2 = e.d2 & 127;
      const beforeAnchor = e.beats < anchor - 1e-9;
      if (beforeAnchor && (range || type !== 11 && type !== 12)) continue;
      if (e.beats > rangeEnd + 1e-9) continue;
      const tick = beforeAnchor ? 0 : capturedEventTick(e.beats, anchor);
      if (tick > lastEventTick) lastEventTick = tick;
      switch (type) {
        case 8: {
          const open = openNotes.get(ch);
          if (open?.has(d1)) {
            ensureChan(ch).push({
              kind: "noteOff",
              channel: ch,
              pitch: d1,
              velocity: d2,
              tick,
              insOrder: insCounter++
            });
            open.delete(d1);
          }
          break;
        }
        case 9: {
          if (d2 === 0) {
            const open = openNotes.get(ch);
            if (open?.has(d1)) {
              ensureChan(ch).push({
                kind: "noteOff",
                channel: ch,
                pitch: d1,
                velocity: 0,
                tick,
                insOrder: insCounter++
              });
              open.delete(d1);
            }
          } else {
            let open = openNotes.get(ch);
            if (!open) {
              open = /* @__PURE__ */ new Set();
              openNotes.set(ch, open);
            }
            if (open.has(d1)) {
              ensureChan(ch).push({
                kind: "noteOff",
                channel: ch,
                pitch: d1,
                velocity: 0,
                tick,
                insOrder: insCounter++
              });
            }
            ensureChan(ch).push({
              kind: "noteOn",
              channel: ch,
              pitch: d1,
              velocity: d2,
              tick,
              insOrder: insCounter++
            });
            open.add(d1);
          }
          break;
        }
        case 11: {
          ensureChan(ch).push({
            kind: "cc",
            channel: ch,
            cn: d1,
            cv: d2,
            tick,
            insOrder: insCounter++
          });
          if (markerLoop.on && tick <= loopStartTick) {
            rememberLoopStartCc(ch, d1, d2);
          }
          break;
        }
        case 12: {
          ensureChan(ch).push({
            kind: "pc",
            channel: ch,
            program: d1,
            tick,
            insOrder: insCounter++,
            clamped: beforeAnchor
          });
          if (markerLoop.on && tick <= loopStartTick) {
            loopStartPcValue.set(ch, d1);
          }
          if (legacyLoopStartReplay && !firstCapturedPcValue.has(ch)) {
            firstCapturedPcValue.set(ch, d1);
          }
          break;
        }
        case 14: {
          const value14 = d2 << 7 | d1;
          const normalized = (value14 - 8192) / 8192;
          ensureChan(ch).push({
            kind: "bend",
            channel: ch,
            bend: normalized,
            tick,
            insOrder: insCounter++
          });
          break;
        }
        default:
          break;
      }
    }
    if (markerLoop.on) {
      if (legacyLoopStartReplay) {
        for (const [ch, prog] of firstCapturedPcValue) {
          if (!loopStartPcValue.has(ch)) loopStartPcValue.set(ch, prog);
        }
      }
      for (const [ch, prog] of loopStartPcValue) {
        ensureChan(ch).push({
          kind: "pc",
          channel: ch,
          program: prog,
          tick: loopStartTick,
          insOrder: insCounter++,
          synthetic: true
        });
      }
      for (const [ch, byCc] of loopStartCcValue) {
        for (const [cc, value] of byCc) {
          ensureChan(ch).push({
            kind: "cc",
            channel: ch,
            cn: cc,
            cv: value,
            tick: loopStartTick,
            insOrder: insCounter++,
            synthetic: true
          });
        }
      }
    }
    const flushTick = lastEventTick + 1;
    for (const [ch, open] of openNotes) {
      for (const pitch of open) {
        ensureChan(ch).push({
          kind: "noteOff",
          channel: ch,
          pitch,
          velocity: 0,
          tick: flushTick,
          insOrder: insCounter++
        });
      }
    }
    const tracks = [conductor];
    const sortedChans = [...pending.keys()].sort((a, b) => a - b);
    for (const ch of sortedChans) {
      const arr = pending.get(ch);
      arr.sort((a, b) => a.tick !== b.tick ? a.tick - b.tick : a.insOrder - b.insOrder);
      const superseded = /* @__PURE__ */ new Set();
      markDuplicateXcmdPairs(arr, superseded);
      const lastIdxByCcAtTick = /* @__PURE__ */ new Map();
      const lastIdxByPcAtTick = /* @__PURE__ */ new Map();
      for (let i = 0; i < arr.length; i++) {
        const p = arr[i];
        if (superseded.has(i)) continue;
        if (p.kind === "pc" && p.clamped) {
          const prev = lastIdxByPcAtTick.get(p.tick);
          if (prev !== void 0) superseded.add(prev);
          lastIdxByPcAtTick.set(p.tick, i);
        }
        if (p.kind === "cc") {
          if (p.cn === XCMD_VALUE_CC || p.cn === XCMD_SELECTOR_CC) continue;
          const key = `${p.cn}|${p.tick}`;
          const prev = lastIdxByCcAtTick.get(key);
          if (prev !== void 0) superseded.add(prev);
          lastIdxByCcAtTick.set(key, i);
        }
      }
      const t = new import_midi_writer_js.default.Track();
      let prevTick = 0;
      let lastCapturedProgram = -1;
      const lastCcValue = /* @__PURE__ */ new Map();
      for (let i = 0; i < arr.length; i++) {
        const p = arr[i];
        if (superseded.has(i)) continue;
        if (p.kind === "pc" && !p.synthetic && p.program === lastCapturedProgram) {
          continue;
        }
        if (p.kind === "cc" && !p.synthetic && p.cn !== XCMD_VALUE_CC && p.cn !== XCMD_SELECTOR_CC && lastCcValue.get(p.cn) === p.cv) {
          continue;
        }
        const delta = Math.max(0, p.tick - prevTick);
        switch (p.kind) {
          case "noteOn":
            t.addEvent(new import_midi_writer_js.default.NoteOnEvent({
              channel: p.channel + 1,
              pitch: p.pitch,
              velocity: p.velocity,
              wait: "T" + delta
            }));
            break;
          case "noteOff":
            t.addEvent(new import_midi_writer_js.default.NoteOffEvent({
              channel: p.channel + 1,
              pitch: p.pitch,
              velocity: p.velocity,
              duration: "T1",
              // unused; buildData reads this.delta late
              delta
            }));
            break;
          case "cc":
            t.addEvent(new import_midi_writer_js.default.ControllerChangeEvent({
              channel: p.channel + 1,
              controllerNumber: p.cn,
              controllerValue: p.cv,
              delta
            }));
            break;
          case "pc":
            t.addEvent(new import_midi_writer_js.default.ProgramChangeEvent({
              channel: p.channel,
              instrument: p.program,
              delta
            }));
            break;
          case "bend":
            t.addEvent(new import_midi_writer_js.default.PitchBendEvent({
              channel: p.channel,
              bend: p.bend,
              delta
            }));
            break;
        }
        if (p.kind === "pc" && !p.synthetic) lastCapturedProgram = p.program;
        if (p.kind === "cc") lastCcValue.set(p.cn, p.cv);
        prevTick = p.tick;
      }
      tracks.push(t);
    }
    const writer = new import_midi_writer_js.default.Writer(tracks, { ticksPerBeat: PPQ });
    return buildWriterFile(writer);
  }

  // code-src/recorder/ccomidi_recorder.ts
  var RECORDER_DIR = "~/Music/poryaaaa-recordings/";
  function resolveSavePath(filename) {
    if (filename.startsWith("/") || filename.startsWith("~")) return filename;
    return RECORDER_DIR + filename;
  }
  function readBufferFileWith(path, openReader) {
    const f = openReader(path);
    if (!f.isopen) {
      throw new Error(`cannot open ${path}`);
    }
    try {
      f.byteorder = "little";
      const magic = f.readbytes(4);
      if (magic.length !== 4 || magic[0] !== 80 || magic[1] !== 82 || magic[2] !== 66 || magic[3] !== 89) {
        throw new Error(`${path}: bad magic`);
      }
      const versionBytes = f.readbytes(2);
      const version = versionBytes[0] | versionBytes[1] << 8;
      if (version !== 1) {
        throw new Error(`${path}: unsupported version ${version}`);
      }
      f.readbytes(2);
      const cb = f.readbytes(8);
      let count = 0;
      for (let i = 7; i >= 0; i--) {
        count = count * 256 + cb[i];
      }
      const events = new Array(count);
      for (let i = 0; i < count; i++) {
        const beatsArr = f.readfloat64(1);
        const tail = f.readbytes(4);
        events[i] = {
          beats: beatsArr[0],
          status: tail[0],
          d1: tail[1],
          d2: tail[2]
        };
      }
      return events;
    } finally {
      f.close();
    }
  }
  function createDumpHandshake(opts) {
    let pending = null;
    const timeoutMs = opts.timeoutMs ?? 1500;
    const timers = globalThis;
    function clearPendingTimer(expected) {
      if (expected.timer !== null && typeof timers.clearTimeout === "function") {
        timers.clearTimeout(expected.timer);
      }
    }
    function displayDumpReason(reason) {
      switch (reason) {
        case "nothing_recorded":
          return "nothing recorded";
        case "export_not_detected":
          return "export not detected";
        case "bad_path":
          return "bad dump path";
        case "write_failed":
          return "dump write failed";
        default:
          return reason || "dump failed";
      }
    }
    return {
      requestBufferDump() {
        if (pending) {
          return Promise.reject(new Error("dump already in flight"));
        }
        const path = opts.tempPath();
        return new Promise((resolve, reject) => {
          let timer = null;
          if (typeof timers.setTimeout === "function") {
            timer = timers.setTimeout(() => {
              if (!pending || pending.path !== path) return;
              pending = null;
              opts.post(`recorder: dump timed out waiting for poryaaaa~ reply path=${path}
`);
              reject(new Error(`dump timed out waiting for poryaaaa~ reply: ${path}`));
            }, timeoutMs);
          } else {
            opts.post("recorder: setTimeout unavailable in this v8 runtime; dump timeout disabled\n");
          }
          pending = { path, resolve, reject, timer };
          opts.post(`recorder: requesting dump from poryaaaa~ path=${path}
`);
          opts.outlet(0, "dump", path);
        });
      },
      dumped(path, count) {
        if (!pending) {
          opts.post("recorder: ignoring unexpected dumped reply\n");
          return;
        }
        const expected = pending;
        pending = null;
        clearPendingTimer(expected);
        if (path !== expected.path) {
          expected.reject(
            new Error(`path mismatch: expected ${expected.path}, got ${path}`)
          );
          return;
        }
        if (count <= 0) {
          expected.reject(new Error("nothing recorded"));
          return;
        }
        expected.resolve({ path, count });
      },
      dumpfailed(path, reason) {
        if (!pending) {
          opts.post("recorder: ignoring unexpected dumpfailed reply\n");
          return;
        }
        const expected = pending;
        pending = null;
        clearPendingTimer(expected);
        if (path && path !== expected.path) {
          expected.reject(
            new Error(`path mismatch: expected ${expected.path}, got ${path}`)
          );
          return;
        }
        expected.reject(new Error(displayDumpReason(reason)));
      },
      isPending() {
        return pending !== null;
      }
    };
  }
  function clampInt(value, lo, hi) {
    if (value === null || value === void 0 || value === "") return null;
    const n = typeof value === "number" ? value : Number(value);
    if (!Number.isFinite(n)) return null;
    const v = Math.floor(n);
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
  }
  function normalizeName(name) {
    return name.toLowerCase().replace(/[^a-z0-9]/g, "");
  }
  function getStringProp(api, prop) {
    try {
      const value = api.getstring(prop);
      if (Array.isArray(value)) return value.map(String).join(" ");
      if (value !== null && value !== void 0) return String(value);
    } catch (_) {
    }
    return "";
  }
  function getNumberProp(api, prop) {
    try {
      const value = api.get(prop);
      const first = Array.isArray(value) ? value[0] : value;
      const n = typeof first === "number" ? first : Number(first);
      return Number.isFinite(n) ? n : null;
    } catch (_) {
      return null;
    }
  }
  function safeGetCount(api, child) {
    try {
      const n = api.getcount(child);
      return Number.isFinite(n) && n > 0 ? Math.floor(n) : 0;
    } catch (_) {
      return 0;
    }
  }
  function isValid(api) {
    const v = api.valid;
    return v === void 0 || v === true || v === 1;
  }
  function parseDictLike(value) {
    if (typeof Dict !== "undefined" && value instanceof Dict) {
      try {
        return JSON.parse(value.stringify());
      } finally {
        value.freepeer();
      }
    }
    if (Array.isArray(value)) {
      if (value.length === 1 && typeof value[0] === "string") {
        const trimmed = value[0].trim();
        if (trimmed.startsWith("{")) return JSON.parse(trimmed);
      }
      for (let i = 0; i < value.length - 1; i += 1) {
        if ((value[i] === "dictionary" || value[i] === "dict") && typeof value[i + 1] === "string") {
          const d = new Dict(value[i + 1]);
          try {
            return JSON.parse(d.stringify());
          } finally {
            d.freepeer();
          }
        }
      }
    }
    if (typeof value === "string") {
      const trimmed = value.trim();
      if (trimmed.startsWith("{")) return JSON.parse(trimmed);
      const d = new Dict(trimmed);
      try {
        return JSON.parse(d.stringify());
      } finally {
        d.freepeer();
      }
    }
    return value;
  }
  function routingChoice(value) {
    const parsed = parseDictLike(value);
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
      throw new Error("routing value is not a dictionary");
    }
    const wrapped = parsed.output_routing_channel;
    if (wrapped) return routingChoice(wrapped);
    const display = parsed.display_name;
    const identifier = parsed.identifier;
    if (typeof display !== "string" || typeof identifier !== "string" && typeof identifier !== "number") {
      throw new Error("routing dictionary missing display_name/identifier");
    }
    return { display_name: display, identifier };
  }
  function routingChoices(value, key) {
    const parsed = parseDictLike(value);
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
      throw new Error(`${key} must be the dictionary returned by track.get("${key}")`);
    }
    const list = parsed[key];
    if (!Array.isArray(list)) throw new Error(`${key} dictionary missing ${key} list`);
    return list.map(routingChoice);
  }
  function parseMidiChannel(choice) {
    const text = choice.display_name;
    if (/track\s*in/i.test(text)) return null;
    const match = text.match(/(?:^|[^0-9])(?:ch(?:annel)?\.?\s*)?([1-9]|1[0-6])(?:[^0-9]|$)/i);
    if (!match) return null;
    return Number(match[1]) - 1;
  }
  function containingTrackPath(devicePath) {
    const match = devicePath.match(/^(live_set tracks \d+)(?:\s|$)/);
    if (!match) throw new Error(`cannot derive track path from ${devicePath}`);
    return match[1];
  }
  function routingChannelForDevice(makeApi, devicePath) {
    const trackPath = containingTrackPath(devicePath);
    const track = makeApi(trackPath);
    if (!isValid(track)) throw new Error(`${trackPath} is not valid`);
    const choice = routingChoice(track.get("output_routing_channel"));
    let channel = null;
    try {
      const available = routingChoices(
        track.get("available_output_routing_channels"),
        "available_output_routing_channels"
      );
      const matched = available.find(
        (candidate) => String(candidate.identifier) === String(choice.identifier)
      );
      if (matched) channel = parseMidiChannel(matched);
    } catch (_) {
    }
    if (channel === null) channel = parseMidiChannel(choice);
    if (channel === null) {
      throw new Error(`${trackPath} output_routing_channel is not a MIDI channel`);
    }
    return channel;
  }
  function parameterMap(makeApi, devicePath) {
    const device = makeApi(devicePath);
    const count = safeGetCount(device, "parameters");
    const out = /* @__PURE__ */ new Map();
    for (let i = 0; i < count; i++) {
      const param = makeApi(`${devicePath} parameters ${i}`);
      if (!isValid(param)) continue;
      const value = getNumberProp(param, "value");
      if (value === null) continue;
      for (const prop of ["name", "original_name"]) {
        const name = getStringProp(param, prop);
        if (!name) continue;
        const key = normalizeName(name);
        if (!out.has(key)) out.set(key, value);
      }
    }
    return out;
  }
  function parameterDebugEntries(makeApi, devicePath) {
    const device = makeApi(devicePath);
    const count = safeGetCount(device, "parameters");
    const out = [];
    for (let i = 0; i < count; i++) {
      const param = makeApi(`${devicePath} parameters ${i}`);
      if (!isValid(param)) continue;
      out.push({
        index: i,
        name: getStringProp(param, "name"),
        originalName: getStringProp(param, "original_name"),
        value: getNumberProp(param, "value")
      });
    }
    return out;
  }
  function logDeviceNode(makeApi, devicePath, log) {
    if (!log) return;
    const device = makeApi(devicePath);
    log(
      `recorder: LOM ccomidi node path=${devicePath} name="${getStringProp(device, "name")}" class_display_name="${getStringProp(device, "class_display_name")}" class_name="${getStringProp(device, "class_name")}"
`
    );
    const params = parameterDebugEntries(makeApi, devicePath);
    if (params.length === 0) {
      log(`recorder: LOM ccomidi params ${devicePath}: <none>
`);
      return;
    }
    for (const param of params) {
      log(
        `recorder: LOM ccomidi param ${devicePath} #${param.index} name="${param.name}" original_name="${param.originalName}" value=${param.value === null ? "<unreadable>" : param.value}
`
      );
    }
  }
  function findParam(params, name) {
    const value = params.get(normalizeName(name));
    return value === void 0 ? null : value;
  }
  function pushCc(out, channel, cc, value) {
    const c = clampInt(cc, 0, 127);
    const v = clampInt(value, 0, 127);
    if (c === null || v === null) return;
    out.push({ channel, cc: c, value: v });
  }
  function pushOptionalCc(out, channel, cc, value, neutral = 0) {
    if (value === null || value === neutral) return;
    pushCc(out, channel, cc, value);
  }
  function pushOptionalXcmd(out, channel, selector, value, neutral = 0) {
    if (value === null || value === neutral) return;
    pushCc(out, channel, 30, selector);
    pushCc(out, channel, 29, value);
  }
  function ccomidiDeviceState(params, channel) {
    const program = clampInt(findParam(params, "VIdx"), 0, 127);
    if (program === null) return null;
    const ccs = [];
    const volume = findParam(params, "Vol");
    const pan = findParam(params, "Pan");
    if (volume !== null) pushCc(ccs, channel, 7, volume);
    if (pan !== null) pushCc(ccs, channel, 10, pan);
    pushOptionalCc(ccs, channel, 1, findParam(params, "Mod"));
    pushOptionalCc(ccs, channel, 21, findParam(params, "LFOSpd"));
    pushOptionalCc(ccs, channel, 26, findParam(params, "LFODly"));
    pushOptionalCc(ccs, channel, 20, findParam(params, "BndRng"));
    pushOptionalCc(ccs, channel, 22, findParam(params, "ModTyp"));
    const tuneSigned = clampInt(findParam(params, "Tune"), -64, 63);
    if (tuneSigned !== null && tuneSigned !== 0) pushCc(ccs, channel, 24, tuneSigned + 64);
    pushOptionalXcmd(ccs, channel, 8, findParam(params, "EchoVol"));
    pushOptionalXcmd(ccs, channel, 9, findParam(params, "EchoLen") ?? findParam(params, "Echo"));
    return { program, ccs };
  }
  function ccValue(ccs, cc) {
    const found = ccs.find((entry) => entry.cc === cc);
    return found ? found.value : null;
  }
  function logFoundState(log, devicePath, channel, state) {
    const volume = ccValue(state.ccs, 7);
    const pan = ccValue(state.ccs, 10);
    log(
      `recorder: LOM ccomidi state ${devicePath} ch${channel + 1} PC=${state.program} CC7=${volume === null ? "<missing>" : volume} CC10=${pan === null ? "<missing>" : pan} CCs=${state.ccs.length}
`
    );
  }
  function logSnapshotSummary(log, snap) {
    log(
      `recorder: LOM ccomidi snapshot devices=${snap.deviceCount} programs=${snap.voicemap.size} CCs=${snap.initialCcs.length} failures=${snap.failures.length}
`
    );
  }
  function isCcomidiDevice(api) {
    const haystack = [
      getStringProp(api, "name"),
      getStringProp(api, "class_display_name"),
      getStringProp(api, "class_name")
    ].join(" ").toLowerCase();
    return haystack.includes("ccomidi");
  }
  function isRackDevice(api) {
    const haystack = [
      getStringProp(api, "name"),
      getStringProp(api, "class_display_name"),
      getStringProp(api, "class_name")
    ].join(" ").toLowerCase();
    return haystack.includes("rack");
  }
  function collectDevicePaths(makeApi, containerPath, childName, out) {
    const container = makeApi(containerPath);
    const count = safeGetCount(container, childName);
    for (let i = 0; i < count; i++) {
      walkDevice(makeApi, `${containerPath} ${childName} ${i}`, out);
    }
  }
  function walkDevice(makeApi, devicePath, out) {
    const device = makeApi(devicePath);
    if (!isValid(device)) return;
    if (isCcomidiDevice(device)) out.push(devicePath);
    if (!isRackDevice(device)) return;
    for (const childName of ["chains", "return_chains"]) {
      const chainCount = safeGetCount(device, childName);
      for (let i = 0; i < chainCount; i++) {
        collectDevicePaths(makeApi, `${devicePath} ${childName} ${i}`, "devices", out);
      }
    }
  }
  function collectCcomidiStateViaLom(makeApi, log) {
    const devicePaths = [];
    const liveSet = makeApi("live_set");
    const trackCount = safeGetCount(liveSet, "tracks");
    for (let trackIndex = 0; trackIndex < trackCount; trackIndex++) {
      collectDevicePaths(makeApi, `live_set tracks ${trackIndex}`, "devices", devicePaths);
    }
    const voicemap = /* @__PURE__ */ new Map();
    const ccsByChannel = /* @__PURE__ */ new Map();
    const failures = [];
    for (const devicePath of devicePaths) {
      logDeviceNode(makeApi, devicePath, log);
      const channel = (() => {
        try {
          return routingChannelForDevice(makeApi, devicePath);
        } catch (e) {
          const reason = String(e);
          failures.push({ path: devicePath, reason });
          if (log) log(`recorder: LOM ccomidi skipped ${devicePath}: ${reason}
`);
          return null;
        }
      })();
      if (channel === null) continue;
      const state = ccomidiDeviceState(parameterMap(makeApi, devicePath), channel);
      if (!state) {
        failures.push({ path: devicePath, reason: "missing or invalid VIdx parameter" });
        if (log) log(`recorder: LOM ccomidi state ${devicePath} ch${channel + 1} missing VIdx
`);
        continue;
      }
      if (log) logFoundState(log, devicePath, channel, state);
      voicemap.set(channel, state.program);
      if (ccsByChannel.has(channel)) ccsByChannel.delete(channel);
      ccsByChannel.set(channel, state.ccs);
    }
    const initialCcs = [];
    for (const ccs of ccsByChannel.values()) initialCcs.push(...ccs);
    const snap = { voicemap, initialCcs, deviceCount: devicePaths.length, failures };
    if (log) logSnapshotSummary(log, snap);
    return snap;
  }
  function createRecorderService(deps) {
    let currentFilename = "";
    let currentStartBeat = "";
    let currentLengthBeats = "";
    let currentLoopStart = "";
    let currentLoopEnd = "";
    let activeSnapshot = null;
    function persistRange() {
      const projectId = deps.getProjectId();
      if (!projectId || !deps.writeRange) return;
      deps.writeRange(projectId, {
        start: currentStartBeat,
        length: currentLengthBeats
      });
    }
    function persistMarkers() {
      const projectId = deps.getProjectId();
      if (!projectId || !deps.writeMarkers) return;
      deps.writeMarkers(projectId, {
        start: currentLoopStart,
        end: currentLoopEnd
      });
    }
    const smfWriter = deps.makeSmfWriter({
      voicemap: () => activeSnapshot?.voicemap ?? /* @__PURE__ */ new Map(),
      initialCcs: () => activeSnapshot?.initialCcs ?? [],
      outputPath: () => currentFilename ? resolveSavePath(currentFilename) : "",
      range: () => ({ start: currentStartBeat, length: currentLengthBeats }),
      markerRange: () => ({ start: currentLoopStart, end: currentLoopEnd })
    });
    return {
      ready() {
        const projectId = deps.getProjectId();
        if (!projectId) {
          deps.post("ccomidi_recorder: Live Set unsaved, no filename to restore\n");
          return;
        }
        const saved = deps.readFilename(projectId);
        if (saved) {
          currentFilename = saved;
          deps.post(`ccomidi_recorder: restored filename "${currentFilename}"
`);
          deps.outlet(1, "set", currentFilename);
        } else {
          deps.post(`ccomidi_recorder: no saved filename for project "${projectId}"
`);
        }
        if (deps.readRange) {
          const r = deps.readRange(projectId);
          if (r) {
            currentStartBeat = r.start ?? "";
            currentLengthBeats = r.length ?? "";
            if (currentStartBeat) deps.outlet(3, "set", currentStartBeat);
            if (currentLengthBeats) deps.outlet(4, "set", currentLengthBeats);
          }
        }
        if (deps.readMarkers) {
          const r = deps.readMarkers(projectId);
          if (r) {
            currentLoopStart = r.start ?? "";
            currentLoopEnd = r.end ?? "";
            if (currentLoopStart) deps.outlet(5, "set", currentLoopStart);
            if (currentLoopEnd) deps.outlet(6, "set", currentLoopEnd);
          }
        }
      },
      setFilename(filename) {
        currentFilename = filename.trim();
        if (!currentFilename) {
          deps.post("ccomidi_recorder: filename cleared\n");
          return;
        }
        const projectId = deps.getProjectId();
        if (projectId) {
          deps.writeFilename(projectId, currentFilename);
          deps.post(`ccomidi_recorder: filename "${currentFilename}" saved to project state
`);
        } else {
          deps.post(`ccomidi_recorder: filename "${currentFilename}" set (Live Set unsaved \u2014 held in memory only)
`);
        }
      },
      getFilename() {
        return currentFilename;
      },
      setStartBeat(s) {
        currentStartBeat = s.trim();
        persistRange();
      },
      setLengthBeats(s) {
        currentLengthBeats = s.trim();
        persistRange();
      },
      setLoopStart(s) {
        currentLoopStart = s.trim();
        persistMarkers();
      },
      setLoopEnd(s) {
        currentLoopEnd = s.trim();
        persistMarkers();
      },
      getStartBeat() {
        return currentStartBeat;
      },
      getLengthBeats() {
        return currentLengthBeats;
      },
      resetStatus() {
        activeSnapshot = null;
        deps.outlet(2, "set", "Ready");
      },
      async save() {
        activeSnapshot = deps.collectCcomidiSnapshot();
        try {
          const saved = await smfWriter.save();
          if (saved) deps.afterSuccessfulSave?.();
        } finally {
          activeSnapshot = null;
        }
      }
    };
  }
  function isMaxRuntime() {
    return typeof outlet === "function" && typeof messnamed === "function";
  }
  function installMaxHandlers() {
    inlets = 1;
    outlets = 7;
    const STATE_PATHS_BY_OS = {
      macintosh: "~/Library/Application Support/poryaaaa/projects.json",
      windows: "~/AppData/Roaming/poryaaaa/projects.json"
    };
    function statePath() {
      return STATE_PATHS_BY_OS[max.os] || null;
    }
    function readFileString(path) {
      const f = new File(path, "read");
      if (!f.isopen) return null;
      try {
        const len = f.eof || 1 << 20;
        return f.readstring(len);
      } finally {
        f.close();
      }
    }
    function writeFileString(path, payload) {
      const f = new File(path, "write");
      if (!f.isopen) {
        post(`ccomidi_recorder: could not open ${path} for write
`);
        return;
      }
      try {
        f.writestring(payload);
      } finally {
        f.close();
      }
    }
    function readAllStates() {
      const path = statePath();
      if (!path) return {};
      const raw = readFileString(path);
      if (!raw) return {};
      try {
        const parsed = JSON.parse(raw);
        if (!parsed || typeof parsed !== "object") return {};
        return parsed;
      } catch (_) {
        return {};
      }
    }
    function writeAllStates(states) {
      const path = statePath();
      if (!path) return;
      const previous = readFileString(path);
      let payload = JSON.stringify(states, null, 2) + "\n";
      if (previous && payload.length < previous.length) {
        payload += " ".repeat(previous.length - payload.length);
      }
      writeFileString(path, payload);
    }
    function getProjectId() {
      try {
        const liveSet = new LiveAPI(null, "live_set");
        const v = liveSet.get("file_path");
        if (Array.isArray(v) && v.length > 0 && typeof v[0] === "string") return v[0];
        if (typeof v === "string") return v;
        return "";
      } catch (_) {
        return "";
      }
    }
    function readFilename(projectId) {
      const all = readAllStates();
      const entry = all[projectId];
      if (!entry || typeof entry !== "object") return null;
      return typeof entry.recorderFilename === "string" ? entry.recorderFilename : null;
    }
    function writeFilename(projectId, filename2) {
      const all = readAllStates();
      if (!all[projectId]) all[projectId] = {};
      all[projectId].recorderFilename = filename2;
      writeAllStates(all);
    }
    function readRange(projectId) {
      const all = readAllStates();
      const entry = all[projectId];
      if (!entry || typeof entry !== "object") return null;
      const start = typeof entry.recorderStart === "string" ? entry.recorderStart : "";
      const length = typeof entry.recorderLength === "string" ? entry.recorderLength : "";
      if (!start && !length) return null;
      return { start, length };
    }
    function writeRange(projectId, range) {
      const all = readAllStates();
      if (!all[projectId]) all[projectId] = {};
      all[projectId].recorderStart = range.start;
      all[projectId].recorderLength = range.length;
      writeAllStates(all);
    }
    function readMarkers(projectId) {
      const all = readAllStates();
      const entry = all[projectId];
      if (!entry || typeof entry !== "object") return null;
      const start = typeof entry.recorderLoopStart === "string" ? entry.recorderLoopStart : "";
      const end = typeof entry.recorderLoopEnd === "string" ? entry.recorderLoopEnd : "";
      if (!start && !end) return null;
      return { start, end };
    }
    function writeMarkers(projectId, markers) {
      const all = readAllStates();
      if (!all[projectId]) all[projectId] = {};
      all[projectId].recorderLoopStart = markers.start;
      all[projectId].recorderLoopEnd = markers.end;
      writeAllStates(all);
    }
    const liveApi = {
      getTempo() {
        try {
          const ls = new LiveAPI(null, "live_set");
          const v = ls.get("tempo");
          return Array.isArray(v) ? Number(v[0]) : Number(v);
        } catch (_) {
          return 120;
        }
      },
      getLoop() {
        try {
          const ls = new LiveAPI(null, "live_set");
          const num = (v) => {
            if (Array.isArray(v)) return Number(v[0]) || 0;
            return Number(v) || 0;
          };
          return {
            on: num(ls.get("loop")) !== 0,
            start: num(ls.get("loop_start")),
            length: num(ls.get("loop_length"))
          };
        } catch (_) {
          return { on: false, start: 0, length: 0 };
        }
      },
      getTimeSig() {
        try {
          const ls = new LiveAPI(null, "live_set");
          const num = (v) => {
            if (Array.isArray(v)) return Number(v[0]) || 0;
            return Number(v) || 0;
          };
          const n = num(ls.get("signature_numerator")) || 4;
          const d = num(ls.get("signature_denominator")) || 4;
          return { num: n, den: d };
        } catch (_) {
          return { num: 4, den: 4 };
        }
      }
    };
    function tempPath() {
      const stamp = Date.now();
      const rand = Math.floor(Math.random() * 1e6);
      return `/tmp/poryaaaa-${stamp}-${rand}.bin`;
    }
    const handshake = createDumpHandshake({
      outlet: (idx, ...args) => outlet(idx, ...args),
      tempPath,
      post: (m) => post(m)
    });
    const requestBufferDump = handshake.requestBufferDump;
    function readBufferFile(path) {
      return readBufferFileWith(path, (p) => {
        const file = new File(p, "read");
        return file;
      });
    }
    function unlinkTemp(path) {
      outlet(0, "unlink", path);
    }
    function writeSmf(path, bytes) {
      const f = new File(path, "write", "MIDI");
      if (!f.isopen) return false;
      try {
        f.byteorder = "little";
        const arr = Array.from(bytes);
        f.writebytes(arr, arr.length);
        return true;
      } finally {
        f.close();
      }
    }
    function collectCcomidiSnapshot() {
      return collectCcomidiStateViaLom(
        (path) => new LiveAPI(null, path),
        (msg) => post(msg)
      );
    }
    let recordArmed = false;
    const service = createRecorderService({
      outlet: (idx, ...args) => outlet(idx, ...args),
      post: (msg) => post(msg),
      getProjectId,
      readFilename,
      writeFilename,
      readRange,
      writeRange,
      readMarkers,
      writeMarkers,
      collectCcomidiSnapshot,
      afterSuccessfulSave: () => {
        if (!recordArmed) return;
        post(`recorder: re-arming poryaaaa~ after successful save
`);
        outlet(0, "record", 1);
      },
      makeSmfWriter: ({ voicemap, initialCcs, outputPath, range, markerRange }) => createSmfWriter({
        post: (m) => post(m),
        status: (m) => outlet(2, "set", m),
        requestBufferDump,
        readBufferFile,
        unlink: unlinkTemp,
        writeSmf,
        liveApi,
        voicemap,
        outputPath,
        range,
        markerRange,
        readInitialCcs: initialCcs,
        // Fire `clear` to poryaaaa~ via outlet 0. The patcher's
        // [route unlink] doesn't match `clear`, so it falls through to
        // poryaaaa~ which resets its MidiBuffer.
        clearBuffer: () => {
          post(`recorder: sending clear to poryaaaa~ via outlet 0
`);
          outlet(0, "clear");
        }
      })
    });
    function ready() {
      service.ready();
    }
    function filename(...args) {
      post(`recorder: filename received raw args=${JSON.stringify(args)}
`);
      if (args.length === 0) return;
      const name = args.map((a) => String(a)).join(" ").trim();
      if (!name) return;
      service.setFilename(name);
    }
    function startbeat(...args) {
      post(`recorder: startbeat received raw args=${JSON.stringify(args)}
`);
      const s = args.map((a) => String(a)).join(" ");
      service.setStartBeat(s);
    }
    function lengthbeats(...args) {
      post(`recorder: lengthbeats received raw args=${JSON.stringify(args)}
`);
      const s = args.map((a) => String(a)).join(" ");
      service.setLengthBeats(s);
    }
    function loopstart(...args) {
      post(`recorder: loopstart received raw args=${JSON.stringify(args)}
`);
      const s = args.map((a) => String(a)).join(" ");
      service.setLoopStart(s);
    }
    function loopend(...args) {
      post(`recorder: loopend received raw args=${JSON.stringify(args)}
`);
      const s = args.map((a) => String(a)).join(" ");
      service.setLoopEnd(s);
    }
    function record(...args) {
      post(`recorder: record received raw args=${JSON.stringify(args)}
`);
      recordArmed = Number(args[0] ?? 0) !== 0;
      service.resetStatus();
    }
    function save() {
      post(`recorder: save clicked currentFilename="${service.getFilename()}"
`);
      service.save().catch((e) => {
        const err = e;
        post(`recorder: save rejected: ${String(e)}
`);
        post(`recorder: save rejected detail name=${String(err.name ?? "")} message=${String(err.message ?? "")}
`);
        if (err.stack) post(`recorder: save rejected stack=${String(err.stack)}
`);
      });
    }
    function dumped(...args) {
      post(`recorder: dumped received raw args=${JSON.stringify(args)}
`);
      const path = String(args[0] ?? "");
      const count = Number(args[1] ?? 0);
      handshake.dumped(path, count);
    }
    function dumpfailed(...args) {
      post(`recorder: dumpfailed received raw args=${JSON.stringify(args)}
`);
      const path = String(args[0] ?? "");
      const reason = String(args[1] ?? "dump_failed");
      handshake.dumpfailed(path, reason);
    }
    const handlers = {
      ready,
      filename,
      startbeat,
      lengthbeats,
      loopstart,
      loopend,
      record,
      save,
      dumped,
      dumpfailed
    };
    Object.assign(globalThis, handlers);
  }
  if (isMaxRuntime()) installMaxHandlers();
})();
