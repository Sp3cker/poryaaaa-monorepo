from reportlab.lib import colors
from reportlab.lib.colors import HexColor
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    BaseDocTemplate,
    Flowable,
    Frame,
    KeepTogether,
    PageBreak,
    PageTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
)


OUTPUT = "output/pdf/poryaaaa_mgba_audio_implementation_diagnosis.pdf"

PAGE_WIDTH, PAGE_HEIGHT = letter
MARGIN_X = 0.68 * inch
MARGIN_TOP = 0.68 * inch
MARGIN_BOTTOM = 0.62 * inch

INK = HexColor("#17202A")
MUTED = HexColor("#53606C")
NAVY = HexColor("#173B57")
TEAL = HexColor("#197C78")
BLUE = HexColor("#2D6A9F")
ORANGE = HexColor("#D26A2E")
RED = HexColor("#B33A3A")
GREEN = HexColor("#23805A")
PALE_BLUE = HexColor("#EAF2F8")
PALE_TEAL = HexColor("#E8F5F3")
PALE_ORANGE = HexColor("#FFF1E8")
PALE_RED = HexColor("#FBECEC")
LIGHT = HexColor("#F5F7F8")
RULE = HexColor("#D5DCE1")
WHITE = colors.white


styles = getSampleStyleSheet()
styles.add(
    ParagraphStyle(
        name="ReportTitle",
        parent=styles["Title"],
        fontName="Helvetica-Bold",
        fontSize=27,
        leading=31,
        textColor=WHITE,
        alignment=TA_LEFT,
        spaceAfter=12,
    )
)
styles.add(
    ParagraphStyle(
        name="ReportSubtitle",
        parent=styles["Normal"],
        fontName="Helvetica",
        fontSize=12,
        leading=17,
        textColor=HexColor("#DDEAF2"),
    )
)
styles.add(
    ParagraphStyle(
        name="H1Custom",
        parent=styles["Heading1"],
        fontName="Helvetica-Bold",
        fontSize=18,
        leading=22,
        textColor=NAVY,
        spaceBefore=0,
        spaceAfter=10,
        keepWithNext=True,
    )
)
styles.add(
    ParagraphStyle(
        name="H2Custom",
        parent=styles["Heading2"],
        fontName="Helvetica-Bold",
        fontSize=12.5,
        leading=15,
        textColor=TEAL,
        spaceBefore=9,
        spaceAfter=5,
        keepWithNext=True,
    )
)
styles.add(
    ParagraphStyle(
        name="BodyCustom",
        parent=styles["BodyText"],
        fontName="Helvetica",
        fontSize=9.4,
        leading=13.4,
        textColor=INK,
        spaceAfter=6,
    )
)
styles.add(
    ParagraphStyle(
        name="BodySmall",
        parent=styles["BodyText"],
        fontName="Helvetica",
        fontSize=8.2,
        leading=11.2,
        textColor=INK,
        spaceAfter=4,
    )
)
styles.add(
    ParagraphStyle(
        name="BulletCustom",
        parent=styles["BodyText"],
        fontName="Helvetica",
        fontSize=9.1,
        leading=12.7,
        leftIndent=13,
        firstLineIndent=-8,
        bulletIndent=3,
        textColor=INK,
        spaceAfter=4,
    )
)
styles.add(
    ParagraphStyle(
        name="Callout",
        parent=styles["BodyText"],
        fontName="Helvetica-Bold",
        fontSize=10.2,
        leading=14.2,
        textColor=NAVY,
        spaceAfter=0,
    )
)
styles.add(
    ParagraphStyle(
        name="CodePath",
        parent=styles["BodyText"],
        fontName="Courier",
        fontSize=7.4,
        leading=10,
        textColor=HexColor("#243643"),
        wordWrap="CJK",
        spaceAfter=3,
    )
)
styles.add(
    ParagraphStyle(
        name="TableHead",
        parent=styles["Normal"],
        fontName="Helvetica-Bold",
        fontSize=7.5,
        leading=9.2,
        textColor=WHITE,
        alignment=TA_LEFT,
    )
)
styles.add(
    ParagraphStyle(
        name="TableBody",
        parent=styles["Normal"],
        fontName="Helvetica",
        fontSize=7.35,
        leading=9.5,
        textColor=INK,
        alignment=TA_LEFT,
    )
)
styles.add(
    ParagraphStyle(
        name="TableBodyBold",
        parent=styles["TableBody"],
        fontName="Helvetica-Bold",
    )
)
styles.add(
    ParagraphStyle(
        name="Caption",
        parent=styles["Normal"],
        fontName="Helvetica-Oblique",
        fontSize=7.7,
        leading=10.2,
        textColor=MUTED,
        spaceBefore=4,
        spaceAfter=6,
    )
)


def p(text, style="BodyCustom"):
    return Paragraph(text, styles[style])


def bullet(text):
    return Paragraph(f"- {text}", styles["BulletCustom"])


def section_title(number, title):
    data = [
        [
            Paragraph(number, ParagraphStyle("SectionNumber", fontName="Helvetica-Bold", fontSize=9, textColor=WHITE)),
            Paragraph(title, styles["H1Custom"]),
        ]
    ]
    table = Table(data, colWidths=[0.38 * inch, 6.72 * inch], hAlign="LEFT")
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (0, 0), TEAL),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("ALIGN", (0, 0), (0, 0), "CENTER"),
                ("LEFTPADDING", (0, 0), (0, 0), 0),
                ("RIGHTPADDING", (0, 0), (0, 0), 0),
                ("TOPPADDING", (0, 0), (0, 0), 7),
                ("BOTTOMPADDING", (0, 0), (0, 0), 7),
                ("LEFTPADDING", (1, 0), (1, 0), 9),
                ("RIGHTPADDING", (1, 0), (1, 0), 0),
                ("TOPPADDING", (1, 0), (1, 0), 0),
                ("BOTTOMPADDING", (1, 0), (1, 0), 0),
            ]
        )
    )
    return table


def callout(text, background=PALE_TEAL, border=TEAL):
    table = Table([[p(text, "Callout")]], colWidths=[7.1 * inch], hAlign="LEFT")
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), background),
                ("BOX", (0, 0), (-1, -1), 0.8, border),
                ("LINEBEFORE", (0, 0), (0, -1), 4, border),
                ("LEFTPADDING", (0, 0), (-1, -1), 12),
                ("RIGHTPADDING", (0, 0), (-1, -1), 12),
                ("TOPPADDING", (0, 0), (-1, -1), 10),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 10),
            ]
        )
    )
    return table


def metric_cards(cards):
    cells = []
    for value, label, color in cards:
        value_style = ParagraphStyle(
            f"MetricValue{value}{label}",
            fontName="Helvetica-Bold",
            fontSize=17,
            leading=19,
            textColor=color,
            alignment=TA_CENTER,
        )
        label_style = ParagraphStyle(
            f"MetricLabel{value}{label}",
            fontName="Helvetica",
            fontSize=7.7,
            leading=10,
            textColor=MUTED,
            alignment=TA_CENTER,
        )
        cells.append([Paragraph(value, value_style), Paragraph(label, label_style)])
    data = [[Table([[cell[0]], [cell[1]]], colWidths=[2.15 * inch]) for cell in cells]]
    table = Table(data, colWidths=[2.32 * inch] * len(cells), hAlign="LEFT")
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), LIGHT),
                ("BOX", (0, 0), (-1, -1), 0.6, RULE),
                ("INNERGRID", (0, 0), (-1, -1), 0.6, RULE),
                ("TOPPADDING", (0, 0), (-1, -1), 9),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 9),
                ("LEFTPADDING", (0, 0), (-1, -1), 4),
                ("RIGHTPADDING", (0, 0), (-1, -1), 4),
            ]
        )
    )
    return table


class PipelineDiagram(Flowable):
    def __init__(self, width=7.1 * inch, height=2.15 * inch):
        super().__init__()
        self.width = width
        self.height = height

    def draw_box(self, canvas, x, y, w, h, fill, title, subtitle):
        canvas.setFillColor(fill)
        canvas.setStrokeColor(fill)
        canvas.roundRect(x, y, w, h, 4, fill=1, stroke=1)
        canvas.setFillColor(WHITE)
        canvas.setFont("Helvetica-Bold", 8.3)
        canvas.drawCentredString(x + w / 2, y + h - 10, title)
        canvas.setFont("Helvetica", 6.7)
        lines = subtitle.split("|")
        for index, line in enumerate(lines):
            canvas.drawCentredString(x + w / 2, y + 15 - index * 8, line)

    def draw_arrow(self, canvas, x1, y, x2):
        canvas.setStrokeColor(MUTED)
        canvas.setFillColor(MUTED)
        canvas.setLineWidth(1.2)
        canvas.line(x1, y, x2 - 5, y)
        canvas.line(x2 - 5, y, x2 - 10, y + 3)
        canvas.line(x2 - 5, y, x2 - 10, y - 3)

    def draw(self):
        c = self.canv
        c.setFont("Helvetica-Bold", 8.5)
        c.setFillColor(RED)
        c.drawString(0, self.height - 12, "Previous implementation")
        c.setFillColor(GREEN)
        c.drawString(0, 0.92 * inch, "Corrected implementation")

        box_w = 1.55 * inch
        box_h = 0.52 * inch
        gap = 0.30 * inch
        starts = [i * (box_w + gap) for i in range(4)]
        top_y = 1.30 * inch
        bottom_y = 0.18 * inch

        old = [
            ("MP2K shadow", "software volume|pace forced to 0"),
            ("Normalized PSG", "phase paused|wrong trigger state"),
            ("Custom mix rate", ">= 131072 Hz|wrong DAC cadence"),
            ("Custom frontend", "Hann/polyphase|separate high-pass"),
        ]
        new = [
            ("ROM CgbSound", "real NRx order|hardware envelope"),
            ("CPU-cycle PSG", "free-running timers|mGBA LFSR"),
            ("SOUNDBIAS DAC", "32768 << cycle|mGBA mix scale"),
            ("mGBA blip_buf", "impulse kernel|integrator + pole"),
        ]
        for index, ((title, subtitle), x) in enumerate(zip(old, starts)):
            self.draw_box(c, x, top_y, box_w, box_h, RED if index != 2 else ORANGE, title, subtitle)
            if index < 3:
                self.draw_arrow(c, x + box_w, top_y + box_h / 2, starts[index + 1])
        for index, ((title, subtitle), x) in enumerate(zip(new, starts)):
            self.draw_box(c, x, bottom_y, box_w, box_h, TEAL if index != 3 else BLUE, title, subtitle)
            if index < 3:
                self.draw_arrow(c, x + box_w, bottom_y + box_h / 2, starts[index + 1])


class ResidualChart(Flowable):
    def __init__(self, width=7.1 * inch, height=2.05 * inch):
        super().__init__()
        self.width = width
        self.height = height
        self.rows = [
            ("se_pc_on SQ1", 19.588, 0.389),
            ("se_door noise", 5.079, 0.015),
            ("Weather voice 81 SQ2", 39.25, 0.140),
        ]

    def draw(self):
        c = self.canv
        left = 1.42 * inch
        chart_w = self.width - left - 0.25 * inch
        max_value = 40.0
        row_gap = 0.55 * inch
        c.setFont("Helvetica", 6.5)
        c.setFillColor(MUTED)
        for tick in [0, 10, 20, 30, 40]:
            x = left + chart_w * tick / max_value
            c.setStrokeColor(RULE)
            c.line(x, 0.22 * inch, x, self.height - 0.22 * inch)
            c.drawCentredString(x, 0.07 * inch, f"{tick}%")

        for index, (name, before, after) in enumerate(self.rows):
            y = self.height - 0.48 * inch - index * row_gap
            c.setFont("Helvetica-Bold", 7.4)
            c.setFillColor(INK)
            c.drawRightString(left - 7, y + 4, name)
            before_w = chart_w * before / max_value
            after_w = chart_w * after / max_value
            c.setFillColor(PALE_RED)
            c.setStrokeColor(RED)
            c.rect(left, y + 8, before_w, 8, fill=1, stroke=1)
            c.setFillColor(RED)
            c.setFont("Helvetica", 6.6)
            c.drawString(left + before_w + 4, y + 9, f"{before:.3f}% before")
            c.setFillColor(PALE_TEAL)
            c.setStrokeColor(GREEN)
            c.rect(left, y - 4, max(after_w, 1.2), 8, fill=1, stroke=1)
            c.setFillColor(GREEN)
            c.drawString(left + max(after_w, 1.2) + 4, y - 3, f"{after:.3f}% after")


class LevelDeltaChart(Flowable):
    def __init__(self, width=7.1 * inch, height=3.7 * inch):
        super().__init__()
        self.width = width
        self.height = height
        self.rows = [
            ("se_door noise", -0.002),
            ("Weather SQ1", -0.005),
            ("Petalburg SQ2", -0.008),
            ("Route 110 SQ2", -0.011),
            ("Birch Lab SQ2", -0.015),
            ("Littleroot SQ2", -0.016),
            ("Fortree SQ2", -0.022),
            ("Encounter Girl SQ2", -0.023),
            ("Weather SQ2", -0.025),
            ("Vs Regi SQ2", -0.081),
            ("Oldale SQ2", -0.094),
            ("Rustboro SQ1", -0.123),
        ]

    def draw(self):
        c = self.canv
        left = 1.55 * inch
        chart_w = self.width - left - 0.55 * inch
        row_h = (self.height - 0.25 * inch) / len(self.rows)
        max_db = 0.15
        for index, (name, value) in enumerate(self.rows):
            y = self.height - (index + 1) * row_h + 3
            c.setFont("Helvetica", 6.9)
            c.setFillColor(INK)
            c.drawRightString(left - 7, y + 1.5, name)
            magnitude = abs(value)
            bar_w = chart_w * magnitude / max_db
            if magnitude <= 0.05:
                fill, stroke = PALE_TEAL, GREEN
            elif magnitude <= 0.5:
                fill, stroke = PALE_ORANGE, ORANGE
            else:
                fill, stroke = PALE_RED, RED
            c.setFillColor(fill)
            c.setStrokeColor(stroke)
            c.rect(left, y, max(bar_w, 1.2), 7, fill=1, stroke=1)
            c.setFillColor(stroke)
            c.setFont("Helvetica-Bold", 6.6)
            c.drawString(left + max(bar_w, 1.2) + 4, y + 0.5, f"{value:.3f} dB")


def defect_table(rows):
    data = [
        [p("Area", "TableHead"), p("What was wrong", "TableHead"), p("Why it mattered", "TableHead"), p("Correction", "TableHead")]
    ]
    for area, wrong, effect, fix in rows:
        data.append(
            [
                p(area, "TableBodyBold"),
                p(wrong, "TableBody"),
                p(effect, "TableBody"),
                p(fix, "TableBody"),
            ]
        )
    table = Table(data, colWidths=[0.98 * inch, 2.0 * inch, 1.9 * inch, 2.22 * inch], repeatRows=1, hAlign="LEFT")
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), NAVY),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [WHITE, LIGHT]),
                ("BOX", (0, 0), (-1, -1), 0.6, RULE),
                ("INNERGRID", (0, 0), (-1, -1), 0.35, RULE),
                ("LEFTPADDING", (0, 0), (-1, -1), 5),
                ("RIGHTPADDING", (0, 0), (-1, -1), 5),
                ("TOPPADDING", (0, 0), (-1, -1), 5),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
            ]
        )
    )
    return table


def page_decor(canvas, doc):
    page = canvas.getPageNumber()
    canvas.saveState()
    if page == 1:
        canvas.setFillColor(NAVY)
        canvas.rect(0, PAGE_HEIGHT - 4.3 * inch, PAGE_WIDTH, 4.3 * inch, fill=1, stroke=0)
        canvas.setFillColor(TEAL)
        canvas.rect(0, PAGE_HEIGHT - 4.42 * inch, PAGE_WIDTH, 0.12 * inch, fill=1, stroke=0)
    else:
        canvas.setStrokeColor(RULE)
        canvas.setLineWidth(0.6)
        canvas.line(MARGIN_X, PAGE_HEIGHT - 0.43 * inch, PAGE_WIDTH - MARGIN_X, PAGE_HEIGHT - 0.43 * inch)
        canvas.setFont("Helvetica-Bold", 7.2)
        canvas.setFillColor(NAVY)
        canvas.drawString(MARGIN_X, PAGE_HEIGHT - 0.33 * inch, "PORYAAAA / mGBA AUDIO PARITY")
        canvas.setFont("Helvetica", 7.2)
        canvas.setFillColor(MUTED)
        canvas.drawRightString(PAGE_WIDTH - MARGIN_X, PAGE_HEIGHT - 0.33 * inch, "Implementation diagnosis")
    canvas.setStrokeColor(RULE)
    canvas.line(MARGIN_X, 0.40 * inch, PAGE_WIDTH - MARGIN_X, 0.40 * inch)
    canvas.setFont("Helvetica", 7)
    canvas.setFillColor(MUTED)
    canvas.drawString(MARGIN_X, 0.25 * inch, "Prepared 2026-07-10 | Working tree based on ac61c604")
    canvas.drawRightString(PAGE_WIDTH - MARGIN_X, 0.25 * inch, f"Page {page}")
    canvas.restoreState()


doc = BaseDocTemplate(
    OUTPUT,
    pagesize=letter,
    leftMargin=MARGIN_X,
    rightMargin=MARGIN_X,
    topMargin=MARGIN_TOP,
    bottomMargin=MARGIN_BOTTOM,
    title="Poryaaaa mGBA Audio Implementation Diagnosis",
    author="OpenAI Codex",
    subject="Confirmed implementation defects, corrections, parity evidence, and remaining validation boundary",
)
frame = Frame(
    MARGIN_X,
    MARGIN_BOTTOM,
    PAGE_WIDTH - 2 * MARGIN_X,
    PAGE_HEIGHT - MARGIN_TOP - MARGIN_BOTTOM,
    id="body",
    leftPadding=0,
    rightPadding=0,
    topPadding=0,
    bottomPadding=0,
)
doc.addPageTemplates([PageTemplate(id="report", frames=[frame], onPage=page_decor)])

story = []

# Cover
story.append(Spacer(1, 0.62 * inch))
story.append(p("PORYAAAA AUDIO ENGINE", "ReportSubtitle"))
story.append(Spacer(1, 0.13 * inch))
story.append(p("What Was Wrong With the mGBA Audio Implementation", "ReportTitle"))
story.append(
    p(
        "A source-backed diagnosis of the PSG, timing, mixing, resampling, and measurement defects that prevented poryaaaa from sounding like the ROM running through mGBA.",
        "ReportSubtitle",
    )
)
story.append(Spacer(1, 1.52 * inch))
story.append(
    callout(
        "Bottom line: the mismatch was not one bad square-wave volume constant. The implementation diverged at several coupled boundaries - MP2K register behavior, hardware envelope timing, oscillator clocking, SOUNDBIAS cadence, output filtering, and the comparison harness itself.",
        background=PALE_BLUE,
        border=BLUE,
    )
)
story.append(Spacer(1, 0.25 * inch))
story.append(
    metric_cards(
        [
            ("12", "confirmed implementation or measurement defects", RED),
            ("19/19", "audible isolated song/channel pairs with a strict local match", TEAL),
            ("8/8", "isolated stereo routing signatures", GREEN),
        ]
    )
)
story.append(Spacer(1, 0.25 * inch))
story.append(p("Scope", "H2Custom"))
story.append(
    p(
        "This report covers the uncommitted parity work in <font name='Courier'>packages/poryaaaa</font> as of 2026-07-10. It distinguishes corrected engine defects from the remaining comparison-indexing and combined PSG/program-wave validation boundary.",
        "BodyCustom",
    )
)
story.append(PageBreak())

# Executive diagnosis
story.append(section_title("01", "Executive diagnosis"))
story.append(Spacer(1, 0.08 * inch))
story.append(
    p(
        "The previous engine produced recognizable MP2K audio, but its PSG output did not pass through the same state transitions, clock domains, or frontend as mGBA. Several local approximations compounded into audible differences, especially in square-channel level and envelope behavior.",
        "BodyCustom",
    )
)
story.append(PipelineDiagram())
story.append(Spacer(1, 0.08 * inch))
story.append(p("The main failure pattern", "H2Custom"))
for text in [
    "MP2K's software shadow envelope was treated as the final audio envelope instead of programming the Game Boy hardware envelope correctly.",
    "Square oscillators were represented by normalized phase increments rather than mGBA's integer GBA CPU-cycle deadlines.",
    "The complete chip mix ran at a convenient internal rate instead of the SOUNDBIAS-selected DAC cadence.",
    "A custom windowed-sinc/high-pass frontend replaced the exact blip_buf path heard from mGBA.",
    "The validation tool originally accepted polarity inversion and selected the strongest convenient window, which could overstate parity.",
]:
    story.append(bullet(text))
story.append(Spacer(1, 0.08 * inch))
story.append(
    callout(
        "The fixes made the isolated waveform path converge. A controlled Weather voice 81 SQ2 pair reached 0.9999990187 correlation, 0.1401% residual, and +0.00114 dB. The three former song-level SQ2 outliers were comparison-indexing artifacts, not engine discrepancies.",
        background=PALE_TEAL,
        border=TEAL,
    )
)
story.append(PageBreak())

# Defect matrix 1
story.append(section_title("02", "Confirmed engine defects"))
story.append(Spacer(1, 0.08 * inch))
engine_rows = [
    (
        "CGB gain",
        "An artificial CGB gain multiplier was present.",
        "It obscured the real PSG/mix scaling and encouraged tuning by ear.",
        "Removed the multiplier and matched mGBA's integer PSG mix scale.",
    ),
    (
        "Track volume",
        "MIDI tracks defaulted to 100 instead of MP2K's full 127.",
        "Tracks without an early CC7 event started too quiet.",
        "Set raw and effective defaults to 127 and retained song-volume scaling.",
    ),
    (
        "NRx2 envelope",
        "Pace was forced to 0 while poryaaaa software-stepped volume.",
        "mGBA's 64 Hz hardware envelope did not run like the ROM expects.",
        "Emit attack/decay/release pace and direction; clock hardware envelopes at 64 Hz.",
    ),
    (
        "Register order",
        "CGB start, pitch, pan, envelope, and trigger writes were in the wrong order.",
        "Repeated writes reached different intermediate hardware states.",
        "Port the observable CgbSound order, including pitch before volume for square/noise.",
    ),
    (
        "Length control",
        "NRx4 length-enable was always cleared.",
        "Voices with nonzero ToneData.length could not expire like the ROM.",
        "Carry nonzero voice length into the hardware control write.",
    ),
    (
        "Trigger/off",
        "Square/noise trigger and oscillator-off behavior was simplified.",
        "Duty/LFSR/envelope state diverged across releases and retriggers.",
        "Use the ROM's trigger-on-volume-write and CgbOscOff register sequences.",
    ),
]
story.append(defect_table(engine_rows))
story.append(Spacer(1, 0.14 * inch))
story.append(p("Why these defects reinforced one another", "H2Custom"))
story.append(
    p(
        "A wrong starting volume could appear to be a mixer problem; a wrong trigger could reset envelope state; a wrong register order could then make the same byte sequence sound different. Correcting only one constant did not address the state machine that produced the audio.",
        "BodyCustom",
    )
)
story.append(p("Primary implementation boundary", "H2Custom"))
story.append(p("packages/poryaaaa/plugin/m4a/m4a_cgb.c", "CodePath"))
story.append(p("packages/poryaaaa/plugin/m4a/m4a_internal.h", "CodePath"))
story.append(PageBreak())

# Defect matrix 2
story.append(section_title("03", "Clocking and frontend defects"))
story.append(Spacer(1, 0.08 * inch))
clock_rows = [
    (
        "Square timer",
        "A normalized 32-bit phase increment represented SQ1/SQ2 timing.",
        "It did not preserve mGBA's exact elapsed CPU-cycle deadline behavior.",
        "Advance duty indices every 16 * (2048 - frequency) GBA CPU cycles.",
    ),
    (
        "Silent phase",
        "Square phase stopped while a channel was disabled.",
        "Later register writes began from the wrong duty index.",
        "Accumulate elapsed timer time across silence and catch up before writes.",
    ),
    (
        "Noise LFSR",
        "Noise trigger/reset and shift behavior used the wrong sequence.",
        "Noise timbre and repeated patterns differed from mGBA.",
        "Use mGBA 0.10.5's width-specific LFSR state and shift/tap order.",
    ),
    (
        "NR52 phase",
        "The post-m4aSoundInit frame sequencer began at step 0.",
        "The first 512 Hz clock did not match mGBA's NR52 enable convention.",
        "Initialize at frame step 7 so the first tick dispatches step 0.",
    ),
    (
        "DAC cadence",
        "PSG/mix used a >= 131072 Hz internal-rate floor.",
        "mGBA samples the complete GBA mix at the SOUNDBIAS cadence.",
        "Render at 32768 << sampling_cycle: 32, 65, 131, or 262 kHz.",
    ),
    (
        "Frontend",
        "A custom Hann/polyphase resampler and separate float high-pass were used.",
        "The band-limit, ringing, clipping, latency, and DC decay differed.",
        "Port mGBA's bundled blip_buf 1.1.0 impulse, integrator, clip, and 511/512 pole.",
    ),
]
story.append(defect_table(clock_rows))
story.append(Spacer(1, 0.16 * inch))
story.append(
    callout(
        "The decisive experiment was not another gain tweak: lowering the DAC cadence to 65536 Hz and using mGBA's blip impulse changed se_truck_stop SQ1 from roughly 0.966 correlation / 25.7% residual to approximately 0.99999 / 0.34% in the initial experiment.",
        background=PALE_BLUE,
        border=BLUE,
    )
)
story.append(Spacer(1, 0.12 * inch))
story.append(p("Primary implementation boundary", "H2Custom"))
story.append(p("packages/poryaaaa/plugin/hw_audio/hw_psg.c", "CodePath"))
story.append(p("packages/poryaaaa/plugin/hw_audio/hw_audio.c", "CodePath"))
story.append(p("packages/poryaaaa/plugin/hw_audio/hw_resample.c", "CodePath"))
story.append(PageBreak())

# Measurement defects
story.append(section_title("04", "The measurement harness was also wrong"))
story.append(Spacer(1, 0.08 * inch))
story.append(
    p(
        "Early comparisons could produce a reassuring number without proving that both files contained the same waveform. That made it possible to compare levels across unrelated phases or envelope states.",
        "BodyCustom",
    )
)
measurement_rows = [
    (
        "Polarity",
        "Lag ranking and thresholds used absolute correlation.",
        "A waveform multiplied by -1 could pass with near-zero fitted residual.",
        "Rank and gate on signed positive correlation; inversion is a failure.",
    ),
    (
        "Window selection",
        "The strongest passing window anywhere in the scan was selected.",
        "A later convenient cycle could hide attack or envelope differences.",
        "Select the earliest audible window that passes the strict shape gate.",
    ),
    (
        "Thresholds",
        "Defaults were 0.98 correlation and 20% residual.",
        "Square-like but materially different windows could authorize a dB claim.",
        "Require >= 0.999 signed correlation and <= 5% residual.",
    ),
    (
        "Activity gate",
        "Only the reference window had to be audibly active.",
        "A nearly silent candidate could still produce a misleading ratio.",
        "Require both windows above the PCM16 activity floor.",
    ),
    (
        "Scope",
        "A local match risked being described as whole-song parity.",
        "Repeated events at different envelope stages produced false song-level dB outliers.",
        "Track a longer musical neighborhood before applying the strict local waveform gate.",
    ),
]
story.append(defect_table(measurement_rows))
story.append(Spacer(1, 0.18 * inch))
story.append(p("Current proof sequence", "H2Custom"))
for text in [
    "For timbre/level checks, fold both stereo files to mono using the arithmetic mean.",
    "Detect audible onset independently in each file.",
    "Track normalized 1.25-to-2-second energy neighborhoods with continuity constraints.",
    "Compare locally around the tracked neighborhood with a bounded sample-lag search.",
    "Remove DC only for shape analysis; do not normalize the captures.",
    "Fit gain and require positive correlation plus low residual before reading raw RMS dB.",
    "For routing checks, compare left and right independently and verify expected silence.",
]:
    story.append(bullet(text))
story.append(PageBreak())

# Evidence
story.append(section_title("05", "Evidence after correction"))
story.append(Spacer(1, 0.08 * inch))
story.append(
    p(
        "The residual chart shows how much of each candidate waveform remained unexplained after fitting one gain scalar. Lower is better. The Weather value is the controlled one-voice SQ2 pair, independent of song allocation and repeated-event indexing.",
        "BodyCustom",
    )
)
story.append(ResidualChart())
story.append(p("Representative before/after gain-fitted residuals. Values use different stages of the evolving harness and are diagnostic rather than a formal benchmark series.", "Caption"))
story.append(
    metric_cards(
        [
            ("775/777", "direct C/C++ tests; two unrelated parity failures remain", NAVY),
            ("12/12", "waveform and coverage tool tests", TEAL),
            ("PASS", "headless mGBA smoke and paired-capture helper", GREEN),
        ]
    )
)
story.append(Spacer(1, 0.16 * inch))
story.append(p("What the fixed baseline demonstrates", "H2Custom"))
story.append(
    p(
        "A ten-song matrix produced strict same-polarity local waveform matches for all 19 audible isolated SQ1/SQ2 pairs. Controlled Weather voice 81 and se_door noise also match at nearly identical raw RMS level. This rules out a universal PSG, SQ2, or noise gain defect.",
        "BodyCustom",
    )
)
story.append(
    callout(
        "The installed CLAP binary and the build artifact matched SHA-256 bf94ffb8dfb791a2c39a427d8c341081c8569f05f9a2f7462d91d044187d775d after the final build.",
        background=LIGHT,
        border=NAVY,
    )
)
story.append(PageBreak())

# Cross-song and stereo evidence
story.append(section_title("06", "Cross-song and stereo evidence"))
story.append(Spacer(1, 0.08 * inch))
story.append(
    p(
        "Ten representative mus_ songs were captured for eight seconds with SQ1 and SQ2 isolated. All 19 audible pairs found strict same-polarity local waveform matches. The chart shows neighborhood-aligned representative deltas; mus_littleroot SQ1 was silent and not comparable.",
        "BodyCustom",
    )
)
story.append(LevelDeltaChart())
story.append(p("Representative raw RMS poryaaaa-to-mGBA deltas after neighborhood alignment and the strict local same-wave gate.", "Caption"))
story.append(
    callout(
        "The former Encounter Girl, Weather Groudon, and Vs Regi SQ2 values of -0.368, -6.085, and -10.137 dB matched different repeated events or envelope stages. Neighborhood alignment gives -0.023, -0.025, and -0.081 dB.",
        background=PALE_BLUE,
        border=BLUE,
    )
)
story.append(Spacer(1, 0.12 * inch))
story.append(
    p(
        "Stereo routing also matches for isolated channels: Weather SQ1 is left-only, Weather SQ2 is right-only, Birch Lab SQ2 is left-only, and Door noise is centered. All 8 routing signatures passed; a deliberate channel swap failed and expected silent sides contained zero PCM.",
        "BodyCustom",
    )
)
story.append(PageBreak())

# Remaining validation boundary
story.append(section_title("07", "Remaining validation boundary"))
story.append(Spacer(1, 0.08 * inch))
story.append(p("What the evidence rules out", "H2Custom"))
for text in [
    "A constant SQ2 gain error: controlled Weather voice 81 is within +0.00114 dB.",
    "Square duty tables or base frequency math: strict same-polarity waveform matches exist across duties 0 through 3.",
    "The global PSG mix and mGBA frontend: SQ1, SQ2, and noise controlled cases converge through the same path.",
    "Stereo collapse or reversal: isolated left-only, right-only, and centered captures reproduce mGBA routing.",
]:
    story.append(bullet(text))
story.append(p("What remains unproven", "H2Custom"))
story.append(
    p(
        "The current coverage reporter can choose different repeated musical events or envelope stages because it scans equal onset-relative offsets and accepts the earliest strict local shape match. The remaining engineering problem is robust musical-neighborhood indexing. The remaining audio-proof boundary is a combined PSG mix that includes the programmable-wave channel.",
        "BodyCustom",
    )
)
story.append(
    Table(
        [
            [p("1", "TableHead"), p("Red fixture", "TableBodyBold"), p("Repeat one square frequency at different envelope levels and expose the false earliest-window match.", "TableBody")],
            [p("2", "TableHead"), p("Neighborhood tracking", "TableBodyBold"), p("Align normalized 1.25-to-2-second energy neighborhoods with continuity constraints.", "TableBody")],
            [p("3", "TableHead"), p("Strict local gate", "TableBodyBold"), p("Require activity, positive correlation >= 0.999, and residual <= 5% near the tracked neighborhood.", "TableBody")],
            [p("4", "TableHead"), p("Combined PSG proof", "TableBodyBold"), p("Compare left and right independently on a mix that includes the programmable-wave channel.", "TableBody")],
        ],
        colWidths=[0.36 * inch, 1.7 * inch, 5.04 * inch],
        hAlign="LEFT",
        style=TableStyle(
            [
                ("BACKGROUND", (0, 0), (0, -1), TEAL),
                ("BACKGROUND", (1, 0), (-1, -1), LIGHT),
                ("TEXTCOLOR", (1, 0), (-1, -1), INK),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("BOX", (0, 0), (-1, -1), 0.6, RULE),
                ("INNERGRID", (0, 0), (-1, -1), 0.35, RULE),
                ("LEFTPADDING", (0, 0), (-1, -1), 6),
                ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                ("TOPPADDING", (0, 0), (-1, -1), 7),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
            ]
        ),
    )
)
story.append(Spacer(1, 0.16 * inch))
story.append(
    callout(
        "Next action: add a red repeated-event indexing fixture, implement the smallest neighborhood-aligned comparison mode, then use it to prove combined PSG/program-wave stereo. Do not reopen SQ2 allocation tracing without new correctly aligned evidence.",
        background=PALE_ORANGE,
        border=ORANGE,
    )
)
story.append(PageBreak())

# Appendix
story.append(section_title("08", "Code map and verification"))
story.append(Spacer(1, 0.08 * inch))
story.append(p("Primary corrected modules", "H2Custom"))
paths = [
    ("MP2K CGB register/envelope behavior", "packages/poryaaaa/plugin/m4a/m4a_cgb.c"),
    ("CGB channel state", "packages/poryaaaa/plugin/m4a/m4a_internal.h"),
    ("PSG oscillator/envelope/frame sequencer", "packages/poryaaaa/plugin/hw_audio/hw_psg.c"),
    ("SOUNDBIAS cadence and frontend orchestration", "packages/poryaaaa/plugin/hw_audio/hw_audio.c"),
    ("mGBA blip_buf port", "packages/poryaaaa/plugin/hw_audio/hw_resample.c"),
    ("Unit regressions", "packages/poryaaaa/test/test_engine.c"),
    ("Headless reference tools", "packages/poryaaaa/tools/mgba-reference/"),
    ("PSG parity handoff", ".scratch/mgba-sq2-parity/CONTEXT.md"),
]
for label, path in paths:
    story.append(
        Table(
            [[p(label, "TableBodyBold"), p(path, "CodePath")]],
            colWidths=[2.2 * inch, 4.9 * inch],
            hAlign="LEFT",
            style=TableStyle(
                [
                    ("VALIGN", (0, 0), (-1, -1), "TOP"),
                    ("LINEBELOW", (0, 0), (-1, -1), 0.35, RULE),
                    ("LEFTPADDING", (0, 0), (-1, -1), 0),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 5),
                    ("TOPPADDING", (0, 0), (-1, -1), 4),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
                ]
            ),
        )
    )
story.append(Spacer(1, 0.12 * inch))
story.append(p("Reference sources used for parity decisions", "H2Custom"))
for text in [
    "Pokemon Emerald/hearth-test compiled m4a implementation, especially src/m4a.c CgbSound and CgbOscOff.",
    "mGBA 0.10.5 src/gb/audio.c and src/gba/audio.c for PSG clocks, envelopes, sampling, mixing, and NR52 behavior.",
    "mGBA 0.10.5 bundled blip_buf 1.1.0 for the output impulse/integrator frontend.",
]:
    story.append(bullet(text))
story.append(p("Latest focused checks", "H2Custom"))
checks = [
    ("Direct engine suite", "775/777; two unrelated voicegroup-core parity failures"),
    ("Comparator tests", "6/6"),
    ("Coverage tests", "6/6"),
    ("Paired-capture helper", "Pass"),
    ("Headless mGBA smoke", "Pass"),
    ("Isolated song matrix", "19/19 audible SQ1/SQ2 pairs with strict local matches"),
    ("Stereo routing", "8/8 isolated routing signatures"),
    ("git diff --check", "Pass"),
]
check_data = [[p("Check", "TableHead"), p("Result", "TableHead")]] + [
    [p(name, "TableBodyBold"), p(result, "TableBody")] for name, result in checks
]
check_table = Table(check_data, colWidths=[2.25 * inch, 4.85 * inch], repeatRows=1, hAlign="LEFT")
check_table.setStyle(
    TableStyle(
        [
            ("BACKGROUND", (0, 0), (-1, 0), NAVY),
            ("ROWBACKGROUNDS", (0, 1), (-1, -1), [WHITE, LIGHT]),
            ("BOX", (0, 0), (-1, -1), 0.6, RULE),
            ("INNERGRID", (0, 0), (-1, -1), 0.35, RULE),
            ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ("LEFTPADDING", (0, 0), (-1, -1), 6),
            ("RIGHTPADDING", (0, 0), (-1, -1), 6),
            ("TOPPADDING", (0, 0), (-1, -1), 5),
            ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
        ]
    )
)
story.append(check_table)
story.append(Spacer(1, 0.14 * inch))
story.append(
    callout(
        "Worktree note: the parity changes are uncommitted and coexist with unrelated modifications in poryaaaa-m4l, voicegroup-core, voicegroup-lsp, and other files. Review and stage surgically.",
        background=LIGHT,
        border=NAVY,
    )
)

doc.build(story)
print(OUTPUT)
