{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 0,
            "revision": 0,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [
            85.0,
            104.0,
            1611.0,
            310.0
        ],
        "bglocked": 0,
        "openinpresentation": 1,
        "default_fontsize": 12.0,
        "default_fontface": 0,
        "default_fontname": "Arial",
        "gridonopen": 1,
        "gridsize": [
            15.0,
            15.0
        ],
        "gridsnaponopen": 1,
        "objectsnaponopen": 1,
        "statusbarvisible": 2,
        "toolbarvisible": 1,
        "lefttoolbarpinned": 0,
        "toptoolbarpinned": 0,
        "righttoolbarpinned": 0,
        "bottomtoolbarpinned": 0,
        "toolbars_unpinned_last_save": 0,
        "tallnewobj": 0,
        "boxanimatetime": 200,
        "enablehscroll": 1,
        "enablevscroll": 1,
        "devicewidth": 220.0,
        "description": "One-dial remote mapper for a selected Vol parameter.",
        "digest": "Remote Vol Mapper",
        "tags": "Max for Live, remote mapping, volume",
        "style": "",
        "subpatcher_template": "",
        "assistshowspatchername": 0,
        "boxes": [
            {
                "box": {
                    "maxclass": "comment",
                    "id": "obj-1",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "outlettype": [],
                    "patching_rect": [
                        1485.0,
                        30.0,
                        86.0,
                        20.0
                    ],
                    "text": "Vol Remote",
                    "fontname": "Arial",
                    "fontsize": 14.0,
                    "presentation": 1,
                    "presentation_rect": [
                        10.0,
                        8.0,
                        112.0,
                        18.0
                    ],
                    "fontface": 1,
                    "textcolor": [
                        0.8,
                        0.8,
                        0.82,
                        1.0
                    ]
                }
            },
            {
                "box": {
                    "maxclass": "live.dial",
                    "id": "obj-2",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [
                        "",
                        ""
                    ],
                    "patching_rect": [
                        30.0,
                        30.0,
                        44.0,
                        66.0
                    ],
                    "parameter_enable": 1,
                    "presentation": 1,
                    "presentation_rect": [
                        18.0,
                        34.0,
                        52.0,
                        70.0
                    ],
                    "varname": "vol",
                    "shownumber": 1,
                    "showname": 1,
                    "annotation": "Remote value sent to the mapped Vol parameter.",
                    "annotation_name": "Vol",
                    "saved_attribute_attributes": {
                        "valueof": {
                            "parameter_initial": [
                                0.75
                            ],
                            "parameter_initial_enable": 1,
                            "parameter_longname": "Vol",
                            "parameter_shortname": "Vol",
                            "parameter_mmin": 0.0,
                            "parameter_mmax": 1.0,
                            "parameter_modmode": 3,
                            "parameter_type": 1,
                            "parameter_unitstyle": 5
                        }
                    }
                }
            },
            {
                "box": {
                    "maxclass": "comment",
                    "id": "obj-3",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "outlettype": [],
                    "patching_rect": [
                        90.0,
                        30.0,
                        40.0,
                        20.0
                    ],
                    "text": "Vol",
                    "fontname": "Arial",
                    "fontsize": 10.0,
                    "presentation": 1,
                    "presentation_rect": [
                        29.0,
                        105.0,
                        34.0,
                        14.0
                    ],
                    "textcolor": [
                        0.8,
                        0.8,
                        0.82,
                        1.0
                    ]
                }
            },
            {
                "box": {
                    "maxclass": "textbutton",
                    "id": "obj-4",
                    "numinlets": 1,
                    "numoutlets": 3,
                    "outlettype": [
                        "",
                        "",
                        ""
                    ],
                    "patching_rect": [
                        315.0,
                        30.0,
                        64.0,
                        24.0
                    ],
                    "parameter_enable": 0,
                    "presentation": 1,
                    "presentation_rect": [
                        84.0,
                        42.0,
                        58.0,
                        22.0
                    ],
                    "text": "Map",
                    "rounded": 4.0,
                    "textcolor": [
                        1.0,
                        1.0,
                        1.0,
                        1.0
                    ],
                    "bgcolor": [
                        0.18,
                        0.32,
                        0.42,
                        1.0
                    ]
                }
            },
            {
                "box": {
                    "maxclass": "comment",
                    "id": "obj-5",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "outlettype": [],
                    "patching_rect": [
                        870.0,
                        240.0,
                        220.0,
                        18.0
                    ],
                    "text": "Unmapped",
                    "fontname": "Arial",
                    "fontsize": 10.0,
                    "presentation": 1,
                    "presentation_rect": [
                        82.0,
                        72.0,
                        128.0,
                        18.0
                    ],
                    "textcolor": [
                        0.8,
                        0.8,
                        0.82,
                        1.0
                    ]
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-6",
                    "numinlets": 1,
                    "numoutlets": 3,
                    "outlettype": [
                        "",
                        "",
                        ""
                    ],
                    "patching_rect": [
                        165.0,
                        30.0,
                        121.0,
                        22.0
                    ],
                    "text": "live.thisdevice",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-7",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        150.0,
                        120.0,
                        72.0,
                        22.0
                    ],
                    "text": "deferlow",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "message",
                    "id": "obj-8",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        360.0,
                        165.0,
                        275.0,
                        22.0
                    ],
                    "text": "path live_set view selected_parameter",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-9",
                    "numinlets": 1,
                    "numoutlets": 3,
                    "outlettype": [
                        "",
                        "",
                        ""
                    ],
                    "patching_rect": [
                        345.0,
                        195.0,
                        310.0,
                        22.0
                    ],
                    "text": "live.path live_set view selected_parameter",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-10",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        1395.0,
                        240.0,
                        44.0,
                        22.0
                    ],
                    "text": "gate",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-11",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [
                        "",
                        ""
                    ],
                    "patching_rect": [
                        45.0,
                        120.0,
                        93.0,
                        22.0
                    ],
                    "text": "trigger b b",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "message",
                    "id": "obj-12",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        135.0,
                        165.0,
                        40.0,
                        22.0
                    ],
                    "text": "1",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "message",
                    "id": "obj-13",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        180.0,
                        165.0,
                        163.0,
                        22.0
                    ],
                    "text": "set Select target Vol",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-14",
                    "numinlets": 1,
                    "numoutlets": 5,
                    "outlettype": [
                        "",
                        "",
                        "",
                        "",
                        ""
                    ],
                    "patching_rect": [
                        30.0,
                        240.0,
                        135.0,
                        22.0
                    ],
                    "text": "trigger b l l l 0",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-15",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        555.0,
                        240.0,
                        198.0,
                        22.0
                    ],
                    "text": "live.remote~ @normalized 1",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-16",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        765.0,
                        240.0,
                        93.0,
                        22.0
                    ],
                    "text": "live.object",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "message",
                    "id": "obj-17",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        270.0,
                        240.0,
                        72.0,
                        22.0
                    ],
                    "text": "get name",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-18",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [
                        "",
                        ""
                    ],
                    "patching_rect": [
                        1110.0,
                        240.0,
                        86.0,
                        22.0
                    ],
                    "text": "route name",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-19",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        1210.0,
                        240.0,
                        170.0,
                        22.0
                    ],
                    "text": "sprintf set Mapped: %s",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-20",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [
                        "",
                        ""
                    ],
                    "patching_rect": [
                        180.0,
                        240.0,
                        72.0,
                        22.0
                    ],
                    "text": "route id",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "newobj",
                    "id": "obj-21",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [
                        "",
                        ""
                    ],
                    "patching_rect": [
                        360.0,
                        240.0,
                        72.0,
                        22.0
                    ],
                    "text": "select 0",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            },
            {
                "box": {
                    "maxclass": "message",
                    "id": "obj-22",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [
                        ""
                    ],
                    "patching_rect": [
                        440.0,
                        240.0,
                        100.0,
                        22.0
                    ],
                    "text": "set Unmapped",
                    "fontname": "Arial",
                    "fontsize": 12.0
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "source": [
                        "obj-6",
                        0
                    ],
                    "destination": [
                        "obj-7",
                        0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-7",
                        0
                    ],
                    "destination": [
                        "obj-8",
                        0
                    ],
                    "midpoints": [
                        186.0,
                        157.0,
                        351.0,
                        157.0,
                        351.0,
                        195.0,
                        367.0,
                        195.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-8",
                        0
                    ],
                    "destination": [
                        "obj-9",
                        0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-9",
                        1
                    ],
                    "destination": [
                        "obj-10",
                        1
                    ],
                    "midpoints": [
                        500.0,
                        232.0,
                        862.0,
                        232.0,
                        862.0,
                        266.0,
                        862.0,
                        232.0,
                        761.0,
                        232.0,
                        761.0,
                        270.0,
                        761.0,
                        232.0,
                        866.0,
                        232.0,
                        866.0,
                        270.0,
                        866.0,
                        232.0,
                        1102.0,
                        232.0,
                        1102.0,
                        270.0,
                        1102.0,
                        232.0,
                        1207.0,
                        232.0,
                        1207.0,
                        270.0,
                        1207.0,
                        232.0,
                        543.0,
                        232.0,
                        543.0,
                        270.0,
                        1432.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-4",
                        0
                    ],
                    "destination": [
                        "obj-11",
                        0
                    ],
                    "midpoints": [
                        97.0,
                        22.0,
                        82.0,
                        22.0,
                        82.0,
                        58.0,
                        91.5,
                        58.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-11",
                        1
                    ],
                    "destination": [
                        "obj-12",
                        0
                    ],
                    "midpoints": [
                        131.0,
                        112.0,
                        142.0,
                        112.0,
                        142.0,
                        150.0,
                        142.0,
                        150.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-12",
                        0
                    ],
                    "destination": [
                        "obj-10",
                        0
                    ],
                    "midpoints": [
                        155.0,
                        157.0,
                        643.0,
                        157.0,
                        643.0,
                        195.0,
                        643.0,
                        157.0,
                        351.0,
                        157.0,
                        351.0,
                        195.0,
                        351.0,
                        187.0,
                        663.0,
                        187.0,
                        663.0,
                        225.0,
                        663.0,
                        232.0,
                        862.0,
                        232.0,
                        862.0,
                        266.0,
                        862.0,
                        232.0,
                        173.0,
                        232.0,
                        173.0,
                        270.0,
                        173.0,
                        232.0,
                        761.0,
                        232.0,
                        761.0,
                        270.0,
                        761.0,
                        232.0,
                        757.0,
                        232.0,
                        757.0,
                        270.0,
                        757.0,
                        232.0,
                        350.0,
                        232.0,
                        350.0,
                        270.0,
                        350.0,
                        232.0,
                        1102.0,
                        232.0,
                        1102.0,
                        270.0,
                        1102.0,
                        232.0,
                        1207.0,
                        232.0,
                        1207.0,
                        270.0,
                        1207.0,
                        232.0,
                        260.0,
                        232.0,
                        260.0,
                        270.0,
                        260.0,
                        232.0,
                        440.0,
                        232.0,
                        440.0,
                        270.0,
                        440.0,
                        232.0,
                        543.0,
                        232.0,
                        543.0,
                        270.0,
                        1402.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-11",
                        0
                    ],
                    "destination": [
                        "obj-13",
                        0
                    ],
                    "midpoints": [
                        52.0,
                        112.0,
                        142.0,
                        112.0,
                        142.0,
                        150.0,
                        142.0,
                        157.0,
                        127.0,
                        157.0,
                        127.0,
                        195.0,
                        187.0,
                        195.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-13",
                        0
                    ],
                    "destination": [
                        "obj-5",
                        0
                    ],
                    "midpoints": [
                        261.5,
                        157.0,
                        643.0,
                        157.0,
                        643.0,
                        195.0,
                        643.0,
                        187.0,
                        663.0,
                        187.0,
                        663.0,
                        225.0,
                        663.0,
                        232.0,
                        547.0,
                        232.0,
                        547.0,
                        270.0,
                        547.0,
                        232.0,
                        757.0,
                        232.0,
                        757.0,
                        270.0,
                        757.0,
                        232.0,
                        350.0,
                        232.0,
                        350.0,
                        270.0,
                        350.0,
                        232.0,
                        260.0,
                        232.0,
                        260.0,
                        270.0,
                        260.0,
                        232.0,
                        440.0,
                        232.0,
                        440.0,
                        270.0,
                        440.0,
                        232.0,
                        543.0,
                        232.0,
                        543.0,
                        270.0,
                        980.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-10",
                        0
                    ],
                    "destination": [
                        "obj-14",
                        0
                    ],
                    "midpoints": [
                        1417.0,
                        232.0,
                        862.0,
                        232.0,
                        862.0,
                        266.0,
                        862.0,
                        232.0,
                        761.0,
                        232.0,
                        761.0,
                        270.0,
                        761.0,
                        232.0,
                        757.0,
                        232.0,
                        757.0,
                        270.0,
                        757.0,
                        232.0,
                        350.0,
                        232.0,
                        350.0,
                        270.0,
                        350.0,
                        232.0,
                        1102.0,
                        232.0,
                        1102.0,
                        270.0,
                        1102.0,
                        232.0,
                        1207.0,
                        232.0,
                        1207.0,
                        270.0,
                        1207.0,
                        232.0,
                        260.0,
                        232.0,
                        260.0,
                        270.0,
                        260.0,
                        232.0,
                        440.0,
                        232.0,
                        440.0,
                        270.0,
                        440.0,
                        232.0,
                        543.0,
                        232.0,
                        543.0,
                        270.0,
                        97.5,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-14",
                        4
                    ],
                    "destination": [
                        "obj-10",
                        0
                    ],
                    "midpoints": [
                        158.0,
                        232.0,
                        862.0,
                        232.0,
                        862.0,
                        266.0,
                        862.0,
                        232.0,
                        761.0,
                        232.0,
                        761.0,
                        270.0,
                        761.0,
                        232.0,
                        757.0,
                        232.0,
                        757.0,
                        270.0,
                        757.0,
                        232.0,
                        350.0,
                        232.0,
                        350.0,
                        270.0,
                        350.0,
                        232.0,
                        1102.0,
                        232.0,
                        1102.0,
                        270.0,
                        1102.0,
                        232.0,
                        1207.0,
                        232.0,
                        1207.0,
                        270.0,
                        1207.0,
                        232.0,
                        260.0,
                        232.0,
                        260.0,
                        270.0,
                        260.0,
                        232.0,
                        440.0,
                        232.0,
                        440.0,
                        270.0,
                        440.0,
                        232.0,
                        543.0,
                        232.0,
                        543.0,
                        270.0,
                        1402.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-14",
                        3
                    ],
                    "destination": [
                        "obj-15",
                        1
                    ],
                    "midpoints": [
                        127.75,
                        232.0,
                        350.0,
                        232.0,
                        350.0,
                        270.0,
                        350.0,
                        232.0,
                        260.0,
                        232.0,
                        260.0,
                        270.0,
                        260.0,
                        232.0,
                        440.0,
                        232.0,
                        440.0,
                        270.0,
                        440.0,
                        232.0,
                        427.0,
                        232.0,
                        427.0,
                        270.0,
                        746.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-14",
                        2
                    ],
                    "destination": [
                        "obj-16",
                        0
                    ],
                    "midpoints": [
                        97.5,
                        232.0,
                        547.0,
                        232.0,
                        547.0,
                        270.0,
                        547.0,
                        232.0,
                        350.0,
                        232.0,
                        350.0,
                        270.0,
                        350.0,
                        232.0,
                        260.0,
                        232.0,
                        260.0,
                        270.0,
                        260.0,
                        232.0,
                        440.0,
                        232.0,
                        440.0,
                        270.0,
                        440.0,
                        232.0,
                        427.0,
                        232.0,
                        427.0,
                        270.0,
                        772.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-14",
                        1
                    ],
                    "destination": [
                        "obj-20",
                        0
                    ],
                    "midpoints": [
                        67.25,
                        251.0,
                        216.0,
                        251.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-14",
                        0
                    ],
                    "destination": [
                        "obj-17",
                        0
                    ],
                    "midpoints": [
                        37.0,
                        232.0,
                        172.0,
                        232.0,
                        172.0,
                        270.0,
                        277.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-17",
                        0
                    ],
                    "destination": [
                        "obj-16",
                        0
                    ],
                    "midpoints": [
                        306.0,
                        232.0,
                        547.0,
                        232.0,
                        547.0,
                        270.0,
                        547.0,
                        232.0,
                        440.0,
                        232.0,
                        440.0,
                        270.0,
                        440.0,
                        232.0,
                        543.0,
                        232.0,
                        543.0,
                        270.0,
                        772.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-16",
                        0
                    ],
                    "destination": [
                        "obj-18",
                        0
                    ],
                    "midpoints": [
                        811.5,
                        232.0,
                        1098.0,
                        232.0,
                        1098.0,
                        266.0,
                        1153.0,
                        266.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-18",
                        0
                    ],
                    "destination": [
                        "obj-19",
                        0
                    ],
                    "midpoints": [
                        1117.0,
                        251.0,
                        1300.0,
                        251.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-19",
                        0
                    ],
                    "destination": [
                        "obj-5",
                        0
                    ],
                    "midpoints": [
                        1300.0,
                        232.0,
                        1102.0,
                        232.0,
                        1102.0,
                        270.0,
                        980.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-20",
                        0
                    ],
                    "destination": [
                        "obj-21",
                        0
                    ],
                    "midpoints": [
                        187.0,
                        232.0,
                        262.0,
                        232.0,
                        262.0,
                        270.0,
                        396.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-21",
                        0
                    ],
                    "destination": [
                        "obj-22",
                        0
                    ],
                    "midpoints": [
                        542.0,
                        267.0,
                        542.0,
                        232.0,
                        442.0,
                        232.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-22",
                        0
                    ],
                    "destination": [
                        "obj-5",
                        0
                    ],
                    "midpoints": [
                        485.0,
                        232.0,
                        761.0,
                        232.0,
                        761.0,
                        270.0,
                        761.0,
                        232.0,
                        757.0,
                        232.0,
                        757.0,
                        270.0,
                        980.0,
                        270.0
                    ]
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-2",
                        1
                    ],
                    "destination": [
                        "obj-15",
                        0
                    ],
                    "midpoints": [
                        67.0,
                        112.0,
                        230.0,
                        112.0,
                        230.0,
                        150.0,
                        230.0,
                        112.0,
                        146.0,
                        112.0,
                        146.0,
                        150.0,
                        146.0,
                        157.0,
                        352.0,
                        157.0,
                        352.0,
                        195.0,
                        352.0,
                        157.0,
                        183.0,
                        157.0,
                        183.0,
                        195.0,
                        183.0,
                        157.0,
                        351.0,
                        157.0,
                        351.0,
                        195.0,
                        351.0,
                        187.0,
                        337.0,
                        187.0,
                        337.0,
                        225.0,
                        337.0,
                        232.0,
                        173.0,
                        232.0,
                        173.0,
                        270.0,
                        173.0,
                        232.0,
                        350.0,
                        232.0,
                        350.0,
                        270.0,
                        350.0,
                        232.0,
                        260.0,
                        232.0,
                        260.0,
                        270.0,
                        260.0,
                        232.0,
                        352.0,
                        232.0,
                        352.0,
                        270.0,
                        352.0,
                        232.0,
                        427.0,
                        232.0,
                        427.0,
                        270.0,
                        562.0,
                        270.0
                    ]
                }
            }
        ],
        "dependency_cache": [],
        "autosave": 0,
        "editing_bgcolor": [
            0.333,
            0.333,
            0.333,
            1.0
        ],
        "locked_bgcolor": [
            0.333,
            0.333,
            0.333,
            1.0
        ]
    }
}