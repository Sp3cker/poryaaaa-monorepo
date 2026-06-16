{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 1,
            "revision": 4,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [ 118.0, 111.0, 1000.0, 780.0 ],
        "openinpresentation": 1,
        "boxes": [
            {
                "box": {
                    "comment": "",
                    "id": "obj-12",
                    "index": 0,
                    "maxclass": "inlet",
                    "numinlets": 0,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 352.0, 90.0, 30.0, 30.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-50",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 8,
                    "outlettype": [ "", "", "", "int", "int", "", "int", "" ],
                    "patching_rect": [ 256.0, 145.0, 90.0, 22.0 ],
                    "text": "midiparse"
                }
            },
            {
                "box": {
                    "id": "obj-51",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "int", "int" ],
                    "patching_rect": [ 256.0, 185.0, 80.0, 22.0 ],
                    "text": "unpack 0 0"
                }
            },
            {
                "box": {
                    "id": "obj-52",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "int" ],
                    "patching_rect": [ 306.0, 225.0, 50.0, 22.0 ],
                    "text": "> 0"
                }
            },
            {
                "box": {
                    "id": "obj-53",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 2,
                    "outlettype": [ "bang", "" ],
                    "patching_rect": [ 306.0, 265.0, 50.0, 22.0 ],
                    "text": "sel 1"
                }
            },
            {
                "box": {
                    "id": "obj-54",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "int" ],
                    "patching_rect": [ 386.0, 185.0, 40.0, 22.0 ],
                    "text": "int"
                }
            },
            {
                "box": {
                    "id": "obj-55",
                    "maxclass": "newobj",
                    "numinlets": 17,
                    "numoutlets": 17,
                    "outlettype": [ "bang", "bang", "bang", "bang", "bang", "bang", "bang", "bang", "bang", "bang", "bang", "bang", "bang", "bang", "bang", "bang", "" ],
                    "patching_rect": [ 256.0, 315.0, 320.0, 22.0 ],
                    "text": "sel 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16"
                }
            },
            {
                "box": {
                    "id": "obj-56",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 251.0, 411.0, 200.0, 20.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 4.0, 1.0, 120.0, 20.0 ],
                    "text": "MIDI ch activity"
                }
            },
            {
                "box": {
                    "id": "obj-57",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 256.0, 355.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 4.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-58",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 284.0, 355.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 20.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-59",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 312.0, 355.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 36.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-60",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 340.0, 355.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 52.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-61",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 368.0, 355.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 68.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-62",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 396.0, 355.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 84.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-63",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 424.0, 355.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 100.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-64",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 452.0, 355.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 116.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-65",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 256.0, 383.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 132.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-66",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 284.0, 383.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 148.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-67",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 312.0, 383.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 164.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-68",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 340.0, 383.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 180.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-69",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 368.0, 383.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 196.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-70",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 396.0, 383.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 212.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-71",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 424.0, 383.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 228.0, 19.0, 14.0, 14.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-72",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 452.0, 383.0, 22.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 244.0, 19.0, 14.0, 14.0 ]
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "destination": [ "obj-50", 0 ],
                    "source": [ "obj-12", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-51", 0 ],
                    "source": [ "obj-50", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-54", 1 ],
                    "source": [ "obj-50", 6 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-52", 0 ],
                    "source": [ "obj-51", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-53", 0 ],
                    "source": [ "obj-52", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-54", 0 ],
                    "source": [ "obj-53", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-55", 0 ],
                    "source": [ "obj-54", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-57", 0 ],
                    "source": [ "obj-55", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-58", 0 ],
                    "source": [ "obj-55", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-59", 0 ],
                    "source": [ "obj-55", 2 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-60", 0 ],
                    "source": [ "obj-55", 3 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-61", 0 ],
                    "source": [ "obj-55", 4 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-62", 0 ],
                    "source": [ "obj-55", 5 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-63", 0 ],
                    "source": [ "obj-55", 6 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-64", 0 ],
                    "source": [ "obj-55", 7 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-65", 0 ],
                    "source": [ "obj-55", 8 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-66", 0 ],
                    "source": [ "obj-55", 9 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-67", 0 ],
                    "source": [ "obj-55", 10 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-68", 0 ],
                    "source": [ "obj-55", 11 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-69", 0 ],
                    "source": [ "obj-55", 12 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-70", 0 ],
                    "source": [ "obj-55", 13 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-71", 0 ],
                    "source": [ "obj-55", 14 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-72", 0 ],
                    "source": [ "obj-55", 15 ]
                }
            }
        ],
        "autosave": 0,
        "oscreceiveudpport": 0
    }
}