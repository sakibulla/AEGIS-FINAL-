"""Generates PDF incident/notification reports.

Uses reportlab (pure-Python) rather than an HTML-to-PDF tool so there's no
external binary (e.g. wkhtmltopdf) to install.
"""
from datetime import datetime, timezone
from io import BytesIO
from typing import Any, Dict, List

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import Paragraph, SimpleDocTemplate, Spacer, Table, TableStyle

_SEVERITY_COLORS = {
    "CRITICAL": colors.HexColor("#d6304a"),
    "HIGH": colors.HexColor("#e08a00"),
    "MEDIUM": colors.HexColor("#c9a227"),
    "LOW": colors.HexColor("#5c7285"),
}


def build_incidents_pdf(incidents: List[Dict[str, Any]], title: str = "A.E.G.I.S. Incident Report") -> bytes:
    buffer = BytesIO()
    doc = SimpleDocTemplate(
        buffer,
        pagesize=A4,
        title=title,
        leftMargin=18 * mm,
        rightMargin=18 * mm,
        topMargin=16 * mm,
        bottomMargin=16 * mm,
    )
    styles = getSampleStyleSheet()
    cell_style = styles["BodyText"]

    elements = [
        Paragraph(title, styles["Title"]),
        Paragraph(
            f"Generated {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M:%S')} UTC "
            f"&mdash; {len(incidents)} incident(s)",
            styles["Normal"],
        ),
        Spacer(1, 10 * mm),
    ]

    if not incidents:
        elements.append(Paragraph("No incidents recorded.", styles["Normal"]))
    else:
        header = ["Time (UTC)", "Bot", "Type", "Severity", "Title", "Message"]
        data: List[List[Any]] = [header]
        severities: List[str] = []

        for inc in incidents:
            ts = inc.get("timestamp")
            ts_str = ts.strftime("%Y-%m-%d %H:%M:%S") if isinstance(ts, datetime) else str(ts)
            data.append(
                [
                    ts_str,
                    inc.get("bot_id", ""),
                    inc.get("type", ""),
                    inc.get("severity", ""),
                    Paragraph(inc.get("title", ""), cell_style),
                    Paragraph(inc.get("message", ""), cell_style),
                ]
            )
            severities.append(inc.get("severity", ""))

        col_widths = [28 * mm, 20 * mm, 22 * mm, 20 * mm, 38 * mm, 46 * mm]
        table = Table(data, colWidths=col_widths, repeatRows=1)

        style_commands = [
            ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#0d1424")),
            ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
            ("FONTSIZE", (0, 0), (-1, -1), 8),
            ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
            ("GRID", (0, 0), (-1, -1), 0.4, colors.grey),
            ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f4f6f8")]),
        ]
        # Tint the severity cell of each row with its own severity color for quick scanning.
        for row_idx, severity in enumerate(severities, start=1):
            color = _SEVERITY_COLORS.get(severity)
            if color is not None:
                style_commands.append(("BACKGROUND", (3, row_idx), (3, row_idx), color))
                style_commands.append(("TEXTCOLOR", (3, row_idx), (3, row_idx), colors.white))

        table.setStyle(TableStyle(style_commands))
        elements.append(table)

    doc.build(elements)
    return buffer.getvalue()
