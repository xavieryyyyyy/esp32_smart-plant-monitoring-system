# Run this helper on the computer to convert the Markdown technical report into a PDF; it never runs
# on the ESP32.
import re,os,html,textwrap,pathlib
# Escape text before placing it inside ReportLab's XML-like paragraph markup.
from xml.sax.saxutils import escape
# Read image dimensions so evidence photos/screenshots keep their original proportions.
from PIL import Image as PILImage
# Import PDF drawing support used by the report's cover, headers, and footers.
from reportlab.pdfgen import canvas
# Import the page-layout objects used for text, tables, figures, and page breaks.
from reportlab.platypus import BaseDocTemplate,PageTemplate,Frame,Paragraph,Spacer,PageBreak,Table,TableStyle,Image,KeepTogether,Preformatted,CondPageBreak
# Build a contents list from section headings discovered during PDF layout.
from reportlab.platypus.tableofcontents import TableOfContents
# Use ReportLab color values for the report's consistent palette.
from reportlab.lib import colors
# Define reusable paragraph styles rather than styling every paragraph independently.
from reportlab.lib.styles import ParagraphStyle
# Import alignment constants available to the PDF layout code.
from reportlab.lib.enums import TA_LEFT,TA_CENTER
# Register the local report fonts with the PDF engine.
from reportlab.pdfbase import pdfmetrics
# Load TrueType fonts for readable, consistently measured PDF text.
from reportlab.pdfbase.ttfonts import TTFont
# Use the standard A4 paper dimensions, measured in PDF points.
from reportlab.lib.pagesizes import A4
# Locate docs/ from this script's path, regardless of the terminal's working directory.
ROOT=pathlib.Path(__file__).resolve().parents[1]
# Keep the repository root available when resolving report assets.
PROJECT=ROOT.parent
# Read report metadata from the Markdown source; utf-8-sig also accepts a UTF-8 byte-order marker.
METADATA=(ROOT/'technical-report.md').read_text(encoding='utf-8-sig')
# Read one named bold metadata field, such as Authors or Course and section, from the source.
def field(name):
    # Match a whole metadata line and escape the requested field name before using it in a regular
    # expression.
    m=re.search(r'^\*\*'+re.escape(name)+r':\*\* (.+)$',METADATA,re.M)
    # Fail if required title-page metadata is missing rather than generating an incomplete cover.
    if not m: raise ValueError('Missing report field: '+name)
    # Return the field's text with surrounding whitespace removed.
    return m.group(1).strip()
# Split the semicolon-separated author list and remove extra spaces from each name.
AUTHORS=[a.strip() for a in field('Authors').split(';')]
# Read the course/section text printed on the title page.
COURSE=field('Course and section')
# Read the report date supplied in the source instead of silently replacing it with the computer
# date.
REPORT_DATE=field('Report date')
# Read the instructor field for the cover.
INSTRUCTOR=field('Instructor')
# Separate A4's width and height for positioning page content.
WIDTH,HEIGHT=A4
# Define left/right/top/bottom margins and calculate the available text width.
L=R=54; TOP=48;BOTTOM=44;TEXT=WIDTH-L-R
# Define the dark heading, green accent, body text, muted text, and rule colors.
NAVY=colors.HexColor('#142832');GREEN=colors.HexColor('#416c5d');INK=colors.HexColor('#202b33');MUTED=colors.HexColor('#596873');RULE=colors.HexColor('#d5dfe1')
# Register the existing Windows Segoe UI font files under short names used throughout this PDF.
for name,file in [('Report','segoeui.ttf'),('Report-Bold','segoeuib.ttf'),('Report-Italic','segoeuii.ttf')]:pdfmetrics.registerFont(TTFont(name,str(pathlib.Path('C:/Windows/Fonts')/file)))
# Connect the normal/bold/italic font names so paragraph emphasis chooses the correct font.
pdfmetrics.registerFontFamily('Report',normal='Report',bold='Report-Bold',italic='Report-Italic',boldItalic='Report-Bold')
# Collect the reusable typography styles used by the parsing/layout steps below.
styles={
# Style ordinary paragraphs with readable font size, line spacing, and paragraph spacing.
'body':ParagraphStyle('body',fontName='Report',fontSize=10,leading=13.5,textColor=INK,spaceAfter=8),
# Style main sections and keep their headings with the following content.
'h1':ParagraphStyle('h1',fontName='Report-Bold',fontSize=17,leading=22,textColor=NAVY,spaceBefore=13,spaceAfter=12,keepWithNext=True),
# Style smaller subsection headings with the report's green accent.
'h2':ParagraphStyle('h2',fontName='Report-Bold',fontSize=11.5,leading=16,textColor=GREEN,spaceBefore=12,spaceAfter=7,keepWithNext=True),
# Style figure captions smaller and italic, with space before and after.
'caption':ParagraphStyle('caption',fontName='Report-Italic',fontSize=8.5,leading=11.5,textColor=MUTED,spaceBefore=7,spaceAfter=13),
# Style table body cells to fit evidence/details while preserving readable line spacing.
'cell':ParagraphStyle('cell',fontName='Report',fontSize=8.4,leading=11.4,textColor=INK),
# Style table heading cells with bold white text on the dark header background.
'th':ParagraphStyle('th',fontName='Report-Bold',fontSize=8.3,leading=11.2,textColor=colors.white),
# Style literal code blocks with a monospaced font and a pale background.
'code':ParagraphStyle('code',fontName='Courier',fontSize=7.5,leading=10,backColor=colors.HexColor('#f2f5f6'),borderPadding=9,spaceBefore=6,spaceAfter=12),
# Style bibliography entries slightly smaller than body paragraphs.
'ref':ParagraphStyle('ref',fontName='Report',fontSize=8.5,leading=12,textColor=INK,spaceAfter=7),
# Indent list text while leaving room for its bullet or numbering marker.
'bullet':ParagraphStyle('bullet',fontName='Report',fontSize=10,leading=13.5,textColor=INK,leftIndent=14,firstLineIndent=-10,spaceAfter=5)
}
# Convert the limited inline Markdown used by this report into ReportLab paragraph markup.
def inline(s):
    # Escape source characters first so ordinary text cannot become unintended markup.
    s=escape(s)
    # Render text between backticks using the monospaced code font.
    s=re.sub(r'`([^`]+)`',lambda m:'<font name="Courier" size="8.5">'+m.group(1)+'</font>',s)
    # Translate double-asterisk emphasis into bold paragraph markup.
    s=re.sub(r'\*\*([^*]+)\*\*',r'<b>\1</b>',s)
    # Official URLs remain clickable; relative evidence links are described by the evidence index.
    # Make official HTTPS references clickable in the exported PDF.
    s=re.sub(r'(https://[^\s<>]+)',lambda m:'<link href="'+m.group(1)+'" color="#315f7a">'+m.group(1)+'</link>',s)
    # Return the converted inline text to the paragraph/caption/table builder.
    return s
# Extend ReportLab's document class to collect headings for PDF navigation and the contents page.
class ReportDoc(BaseDocTemplate):
    # Run after each laid-out object so main headings can be registered with their actual page
    # number.
    def afterFlowable(self,f):
        # Only main-section paragraphs become top-level contents/bookmark entries.
        if isinstance(f,Paragraph) and f.style.name=='h1':
            # Create a stable lowercase bookmark key from the heading text.
            text=f.getPlainText();key='section-'+re.sub('[^a-z0-9]+','-',text.lower()).strip('-')
            # Add a page bookmark and a corresponding entry in the PDF reader's outline panel.
            self.canv.bookmarkPage(key);self.canv.addOutlineEntry(text,key,0,False)
            # Notify the contents builder of the heading title and its resolved page number.
            self.notify('TOCEntry',(0,text,self.page,key))
# Draw the repeated header/footer decoration for ordinary report pages.
def decor(c,doc):
    # Skip this decoration on the title page, which has its own cover layout.
    if doc.page==1:return
    # Save drawing settings and draw the light separator rules at the top/bottom margins.
    c.saveState();c.setStrokeColor(RULE);c.setLineWidth(.6);c.line(L,HEIGHT-34,WIDTH-R,HEIGHT-34);c.line(L,35,WIDTH-R,35)
    # Draw the project title and course/section in the page header.
    c.setFont('Report',8);c.setFillColor(MUTED);c.drawString(L,HEIGHT-25,'Smart Plant Monitoring System');c.drawRightString(WIDTH-R,HEIGHT-25,'BCA152 Microcontrollers | B186')
    # Draw the author names and current page number, then restore drawing settings.
    c.drawString(L,22,'Paraguya · Restauro · Bermudo');c.drawRightString(WIDTH-R,22,'Page '+str(doc.page));c.restoreState()
# Draw the custom title page using the Markdown metadata.
def cover(c,doc):
    # Draw the dark cover panel and the course/section label.
    c.saveState();c.setFillColor(NAVY);c.rect(0,HEIGHT-245,WIDTH,245,fill=1,stroke=0);c.setFillColor(colors.HexColor('#b7d4c6'));c.setFont('Report',10);c.drawString(L,HEIGHT-72,COURSE.upper())
    # Draw the main project title and its descriptive subtitle.
    c.setFillColor(colors.white);c.setFont('Report-Bold',30);c.drawString(L,HEIGHT-126,'Smart Plant');c.drawString(L,HEIGHT-164,'Monitoring System');c.setFont('Report',11);c.setFillColor(colors.HexColor('#d9e6e8'));c.drawString(L,HEIGHT-201,'Using ESP32, FreeRTOS, and Internet Communication Services')
    # Draw the technical-report label before the author list.
    c.setFillColor(INK);c.setFont('Report-Bold',12);c.drawString(L,HEIGHT-290,'Technical report · Project No. 1');c.setFont('Report',12)
    # Position each author on a separate line with equal vertical spacing.
    for i,name in enumerate(AUTHORS):c.drawString(L,HEIGHT-338-i*23,name)
    # Switch to muted text for the institution/instructor/date details.
    c.setFillColor(MUTED);c.setFont('Report',10)
    # Collect the institution and source-supplied report metadata for the lower cover.
    lines=['Mindanao State University – Iligan Institute of Technology','College of Computer Studies','Department of Computer Applications','','Instructor: '+INSTRUCTOR,'Report date: '+REPORT_DATE]
    # Choose the initial vertical position for those cover details.
    y=HEIGHT-448
    # Draw each detail line, moving down by nineteen PDF points each time.
    for line in lines:c.drawString(L,y,line);y-=19
    # Draw the cover footer and the explicitly named historical evidence checkpoint.
    c.setStrokeColor(RULE);c.line(L,124,WIDTH-R,124);c.setFont('Report',8.5);c.drawString(L,103,'Documentation and evidence checkpoint: 67f1157');c.setFont('Report',8);c.drawString(L,84,'Repository: github.com/xavieryyyyyy/esp32_smart-plant-monitoring-system');c.drawString(L,65,'Adaptations and verification limits are identified in the report.');c.restoreState()
# Resolve and size one Markdown figure and keep its caption with it.
def figure(alt,ref):
    # Try the image path relative to docs/ first.
    file=ROOT/ref
    # Also support the repository/docs asset form already used by this report.
    if not file.exists():file=PROJECT/'docs'/ref
    # Stop if the evidence/figure file is missing instead of silently leaving an empty figure.
    if not file.exists():raise FileNotFoundError(file)
    # Read original image width and height without stretching it.
    with PILImage.open(file) as im:w,h=im.size
    # Preserve original evidence proportions; diagrams get generous dedicated space.
    # Choose a height limit appropriate for a tall diagram, wiring photo, or recipient screenshot.
    maxh=560 if 'processing-flow' in ref else 400 if 'task-architecture' in ref else 230 if 'hardware-setup' in ref else 220 if 'wiring' in ref else 275 if 'final-gmail' in ref else 225 if 'final-' in ref else 245
    # Choose the smaller width/height scale so the figure fits without changing its proportions.
    scale=min(TEXT/w,maxh/h)
    # Create and center the scaled PDF image.
    pic=Image(str(file),width=w*scale,height=h*scale);pic.hAlign='CENTER'
    # Keep the image, small spacer, and its caption together on a page.
    return KeepTogether([Spacer(1,5),pic,Paragraph(inline(alt),styles['caption'])])
# Build one report table from already parsed Markdown rows.
def table(rows):
    # Require the same column count in every row before calculating widths.
    n=len(rows[0]);assert all(len(r)==n for r in rows),rows
    # Choose two-column proportions based on the content of the first header.
    if n==2:rat=[.62,.38] if rows[0][0]=='Repository record' else [.40,.60]
    # Select three-column widths appropriate to the report's test/timing/detail tables.
    elif n==3:
        # Give long test names or descriptions extra width while leaving numeric/result columns
        # narrower.
        rat=[.46,.12,.42] if rows[0][0]=='Test' else [.27,.29,.44] if any('Elapsed' in x for x in rows[0]) else [.28,.38,.34]
    # Choose four-column proportions for task tables or general comparison tables.
    elif n==4:rat=[.19,.10,.18,.53] if rows[0][0]=='Task' else [.23,.15,.28,.34]
    # Use equal widths if this table has another number of columns.
    else:rat=[1/n]*n
    # Turn each cell into a wrapping paragraph; use a different style for the header row.
    cells=[[Paragraph(inline(v),styles['th' if ri==0 else 'cell']) for v in row] for ri,row in enumerate(rows)]
    # Create the table, repeat its header after page splits, and allow splitting between rows.
    t=Table(cells,colWidths=[TEXT*r for r in rat],repeatRows=1,hAlign='LEFT',splitByRow=1)
    # Apply header shading, readable padding, separators, and alternating body row backgrounds.
    t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),NAVY),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),8),('RIGHTPADDING',(0,0),(-1,-1),8),('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5),('LINEBELOW',(0,0),(-1,0),.6,NAVY),('LINEBELOW',(0,1),(-1,-1),.35,RULE),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white,colors.HexColor('#f5f7f8')])]))
    # Return the table with surrounding vertical spacing for insertion into the document story.
    return [Spacer(1,4),t,Spacer(1,12)]
# Read the report body from the Markdown source.
source=(ROOT/'technical-report.md').read_text(encoding='utf-8-sig')
# Begin at Abstract; cover metadata above it is already used by the custom cover.
source=source[source.index('## Abstract'):]
# Prepare source lines, the ordered PDF story, the parser cursor, and figure/table counters.
lines=source.splitlines();story=[Spacer(1,1),PageBreak()];i=0;table_count=0;figure_count=0
# Walk through all Markdown lines, adding the corresponding layout objects to the story.
while i<len(lines):
    # Strip outer whitespace when recognizing the current Markdown construct.
    line=lines[i].strip()
    # Skip blank source lines; paragraph spacing comes from the selected styles.
    if not line:i+=1;continue
    # Honor explicit page-break markers written in the report source.
    if line=='<!-- pagebreak -->':story.append(PageBreak());i+=1;continue
    # Recognize the placeholder where the generated contents page belongs.
    if line=='<!-- toc -->':
        # Add the contents heading and automatically collected section/page entries.
        story.append(Paragraph('Contents',styles['h2']));toc=TableOfContents();toc.levelStyles=[ParagraphStyle('toc',fontName='Report',fontSize=9,leading=13,leftIndent=0,firstLineIndent=0,spaceBefore=3,textColor=INK)];story.append(toc);i+=1;continue
    # Turn a three-hash heading into a subsection and avoid leaving it alone at a page bottom.
    if line.startswith('### '):story.extend([CondPageBreak(55),Paragraph(inline(line[4:]),styles['h2'])]);i+=1;continue
    # Turn a two-hash heading into a main section and reserve a little following-content space.
    if line.startswith('## '):story.extend([CondPageBreak(75),Paragraph(inline(line[3:]),styles['h1'])]);i+=1;continue
    # Recognize Markdown image syntax and capture its caption text and file reference.
    m=re.match(r'!\[(.*?)\]\((.*?)\)',line)
    # Add that sized figure/caption, count it, and move to the next source line.
    if m:story.append(figure(m.group(1),m.group(2)));figure_count+=1;i+=1;continue
    # Recognize the start of a fenced literal code block.
    if line.startswith('```'):
        # Start collecting code lines after the opening fence.
        code=[];i+=1
        # Keep collecting literal lines until the closing code fence.
        while i<len(lines) and not lines[i].strip().startswith('```'):code.append(lines[i]);i+=1
        # Prepare a separate list for lines wrapped to the report's code-block width.
        wrapped=[]
        # Wrap long code lines while preserving their intentional spacing.
        for row in code:wrapped.extend(textwrap.wrap(row,width=100,replace_whitespace=False,drop_whitespace=False) or [''])
        # Add the literal code block, pass the closing fence, and resume Markdown parsing.
        story.append(Preformatted('\n'.join(wrapped),styles['code']));i+=1;continue
    # Recognize a pipe-delimited Markdown table.
    if line.startswith('|'):
        # Start collecting this table's header/body rows.
        rows=[]
        # Keep reading while adjacent lines still belong to the table.
        while i<len(lines) and lines[i].strip().startswith('|'):
            # Split the row into cells after removing its optional outside pipes.
            cells=[v.strip() for v in lines[i].strip().strip('|').split('|')]
            # Ignore the Markdown alignment/separator row; retain actual header/body content.
            if not all(re.fullmatch(r':?-+:?',v.replace(' ','')) for v in cells):rows.append(cells)
            # Advance the source cursor after each table row.
            i+=1
        # Build/add the collected table, count it, then continue after the table.
        story.extend(table(rows));table_count+=1;continue
    # Recognize a simple numbered or dash-bulleted list item.
    if re.match(r'^(\d+\.|-) ',line):
        # Keep its numbering/bullet marker separate from the paragraph text.
        marker,content=line.split(' ',1);story.append(Paragraph(inline(content),styles['bullet'],bulletText='•' if marker=='-' else marker));i+=1;continue
    # Start an ordinary paragraph with this unrecognized-as-special line.
    para=[line];i+=1
    # Join following ordinary lines until a blank line or a new Markdown construct starts.
    while i<len(lines) and lines[i].strip() and not re.match(r'^(#{2,3} |\||```|!\[|<!--|\d+\. |[-] )',lines[i].strip()):para.append(lines[i].strip());i+=1
    # Use bibliography styling for numbered references; otherwise add an ordinary body paragraph.
    text=' '.join(para);sty=styles['ref'] if re.match(r'^\[\d+\]',text) else styles['body'];story.append(Paragraph(inline(text),sty))
# Create the output document with A4 margins and source-derived title/author metadata.
output=ROOT/'technical-report.pdf';doc=ReportDoc(str(output),pagesize=A4,rightMargin=R,leftMargin=L,topMargin=TOP,bottomMargin=BOTTOM,title='Smart Plant Monitoring System',author='; '.join(AUTHORS),subject='BCA152 Microcontrollers — B186: technical report',pageCompression=1)
# Define the usable text frame inside the page margins.
frame=Frame(L,BOTTOM,TEXT,HEIGHT-TOP-BOTTOM,leftPadding=0,rightPadding=0,topPadding=0,bottomPadding=0)
# Attach the cover/ordinary-page drawing callback and build enough passes to resolve contents pages.
doc.addPageTemplates([PageTemplate(id='report',frames=[frame],onPage=lambda c,d: cover(c,d) if d.page==1 else decor(c,d))]);doc.multiBuild(story)
# Print the output path and figure/table counts so a successful generation can be inspected.
print('Built',output,'Figures',figure_count,'Tables',table_count)

