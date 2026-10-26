import re,os,html,textwrap,pathlib
from xml.sax.saxutils import escape
from PIL import Image as PILImage
from reportlab.pdfgen import canvas
from reportlab.platypus import BaseDocTemplate,PageTemplate,Frame,Paragraph,Spacer,PageBreak,Table,TableStyle,Image,KeepTogether,Preformatted,CondPageBreak
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT,TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.pagesizes import A4
ROOT=pathlib.Path(__file__).resolve().parents[1]
PROJECT=ROOT.parent
METADATA=(ROOT/'technical-report.md').read_text(encoding='utf-8-sig')
def field(name):
    m=re.search(r'^\*\*'+re.escape(name)+r':\*\* (.+)$',METADATA,re.M)
    if not m: raise ValueError('Missing report field: '+name)
    return m.group(1).strip()
AUTHORS=[a.strip() for a in field('Authors').split(';')]
COURSE=field('Course and section')
REPORT_DATE=field('Report date')
INSTRUCTOR=field('Instructor')
WIDTH,HEIGHT=A4
L=R=54; TOP=48;BOTTOM=44;TEXT=WIDTH-L-R
NAVY=colors.HexColor('#142832');GREEN=colors.HexColor('#416c5d');INK=colors.HexColor('#202b33');MUTED=colors.HexColor('#596873');RULE=colors.HexColor('#d5dfe1')
for name,file in [('Report','segoeui.ttf'),('Report-Bold','segoeuib.ttf'),('Report-Italic','segoeuii.ttf')]:pdfmetrics.registerFont(TTFont(name,str(pathlib.Path('C:/Windows/Fonts')/file)))
pdfmetrics.registerFontFamily('Report',normal='Report',bold='Report-Bold',italic='Report-Italic',boldItalic='Report-Bold')
styles={
'body':ParagraphStyle('body',fontName='Report',fontSize=10,leading=13.5,textColor=INK,spaceAfter=8),
'h1':ParagraphStyle('h1',fontName='Report-Bold',fontSize=17,leading=22,textColor=NAVY,spaceBefore=13,spaceAfter=12,keepWithNext=True),
'h2':ParagraphStyle('h2',fontName='Report-Bold',fontSize=11.5,leading=16,textColor=GREEN,spaceBefore=12,spaceAfter=7,keepWithNext=True),
'caption':ParagraphStyle('caption',fontName='Report-Italic',fontSize=8.5,leading=11.5,textColor=MUTED,spaceBefore=7,spaceAfter=13),
'cell':ParagraphStyle('cell',fontName='Report',fontSize=8.4,leading=11.4,textColor=INK),
'th':ParagraphStyle('th',fontName='Report-Bold',fontSize=8.3,leading=11.2,textColor=colors.white),
'code':ParagraphStyle('code',fontName='Courier',fontSize=7.5,leading=10,backColor=colors.HexColor('#f2f5f6'),borderPadding=9,spaceBefore=6,spaceAfter=12),
'ref':ParagraphStyle('ref',fontName='Report',fontSize=8.5,leading=12,textColor=INK,spaceAfter=7),
'bullet':ParagraphStyle('bullet',fontName='Report',fontSize=10,leading=13.5,textColor=INK,leftIndent=14,firstLineIndent=-10,spaceAfter=5)
}
def inline(s):
    s=escape(s)
    s=re.sub(r'`([^`]+)`',lambda m:'<font name="Courier" size="8.5">'+m.group(1)+'</font>',s)
    s=re.sub(r'\*\*([^*]+)\*\*',r'<b>\1</b>',s)
    # Official URLs remain clickable; relative evidence links are described by the evidence index.
    s=re.sub(r'(https://[^\s<>]+)',lambda m:'<link href="'+m.group(1)+'" color="#315f7a">'+m.group(1)+'</link>',s)
    return s
class ReportDoc(BaseDocTemplate):
    def afterFlowable(self,f):
        if isinstance(f,Paragraph) and f.style.name=='h1':
            text=f.getPlainText();key='section-'+re.sub('[^a-z0-9]+','-',text.lower()).strip('-')
            self.canv.bookmarkPage(key);self.canv.addOutlineEntry(text,key,0,False)
            self.notify('TOCEntry',(0,text,self.page,key))
def decor(c,doc):
    if doc.page==1:return
    c.saveState();c.setStrokeColor(RULE);c.setLineWidth(.6);c.line(L,HEIGHT-34,WIDTH-R,HEIGHT-34);c.line(L,35,WIDTH-R,35)
    c.setFont('Report',8);c.setFillColor(MUTED);c.drawString(L,HEIGHT-25,'Smart Plant Monitoring System');c.drawRightString(WIDTH-R,HEIGHT-25,'BCA152 Microcontrollers | B186')
    c.drawString(L,22,'Paraguya · Restauro · Bermudo');c.drawRightString(WIDTH-R,22,'Page '+str(doc.page));c.restoreState()
def cover(c,doc):
    c.saveState();c.setFillColor(NAVY);c.rect(0,HEIGHT-245,WIDTH,245,fill=1,stroke=0);c.setFillColor(colors.HexColor('#b7d4c6'));c.setFont('Report',10);c.drawString(L,HEIGHT-72,COURSE.upper())
    c.setFillColor(colors.white);c.setFont('Report-Bold',30);c.drawString(L,HEIGHT-126,'Smart Plant');c.drawString(L,HEIGHT-164,'Monitoring System');c.setFont('Report',11);c.setFillColor(colors.HexColor('#d9e6e8'));c.drawString(L,HEIGHT-201,'Using ESP32, FreeRTOS, and Internet Communication Services')
    c.setFillColor(INK);c.setFont('Report-Bold',12);c.drawString(L,HEIGHT-290,'Technical report · Project No. 1');c.setFont('Report',12)
    for i,name in enumerate(AUTHORS):c.drawString(L,HEIGHT-338-i*23,name)
    c.setFillColor(MUTED);c.setFont('Report',10)
    lines=['Mindanao State University – Iligan Institute of Technology','College of Computer Studies','Department of Computer Applications','','Instructor: '+INSTRUCTOR,'Report date: '+REPORT_DATE]
    y=HEIGHT-448
    for line in lines:c.drawString(L,y,line);y-=19
    c.setStrokeColor(RULE);c.line(L,124,WIDTH-R,124);c.setFont('Report',8.5);c.drawString(L,103,'Documentation and evidence checkpoint: 67f1157');c.setFont('Report',8);c.drawString(L,84,'Repository: github.com/xavieryyyyyy/esp32_smart-plant-monitoring-system');c.drawString(L,65,'Adaptations and verification limits are identified in the report.');c.restoreState()
def figure(alt,ref):
    file=ROOT/ref
    if not file.exists():file=PROJECT/'docs'/ref
    if not file.exists():raise FileNotFoundError(file)
    with PILImage.open(file) as im:w,h=im.size
    # Preserve original evidence proportions; diagrams get generous dedicated space.
    maxh=560 if 'processing-flow' in ref else 400 if 'task-architecture' in ref else 230 if 'hardware-setup' in ref else 220 if 'wiring' in ref else 275 if 'final-gmail' in ref else 225 if 'final-' in ref else 245
    scale=min(TEXT/w,maxh/h)
    pic=Image(str(file),width=w*scale,height=h*scale);pic.hAlign='CENTER'
    return KeepTogether([Spacer(1,5),pic,Paragraph(inline(alt),styles['caption'])])
def table(rows):
    n=len(rows[0]);assert all(len(r)==n for r in rows),rows
    if n==2:rat=[.62,.38] if rows[0][0]=='Repository record' else [.40,.60]
    elif n==3:
        rat=[.46,.12,.42] if rows[0][0]=='Test' else [.27,.29,.44] if any('Elapsed' in x for x in rows[0]) else [.28,.38,.34]
    elif n==4:rat=[.19,.10,.18,.53] if rows[0][0]=='Task' else [.23,.15,.28,.34]
    else:rat=[1/n]*n
    cells=[[Paragraph(inline(v),styles['th' if ri==0 else 'cell']) for v in row] for ri,row in enumerate(rows)]
    t=Table(cells,colWidths=[TEXT*r for r in rat],repeatRows=1,hAlign='LEFT',splitByRow=1)
    t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),NAVY),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),8),('RIGHTPADDING',(0,0),(-1,-1),8),('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5),('LINEBELOW',(0,0),(-1,0),.6,NAVY),('LINEBELOW',(0,1),(-1,-1),.35,RULE),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white,colors.HexColor('#f5f7f8')])]))
    return [Spacer(1,4),t,Spacer(1,12)]
source=(ROOT/'technical-report.md').read_text(encoding='utf-8-sig')
source=source[source.index('## Abstract'):]
lines=source.splitlines();story=[Spacer(1,1),PageBreak()];i=0;table_count=0;figure_count=0
while i<len(lines):
    line=lines[i].strip()
    if not line:i+=1;continue
    if line=='<!-- pagebreak -->':story.append(PageBreak());i+=1;continue
    if line=='<!-- toc -->':
        story.append(Paragraph('Contents',styles['h2']));toc=TableOfContents();toc.levelStyles=[ParagraphStyle('toc',fontName='Report',fontSize=9,leading=13,leftIndent=0,firstLineIndent=0,spaceBefore=3,textColor=INK)];story.append(toc);i+=1;continue
    if line.startswith('### '):story.extend([CondPageBreak(55),Paragraph(inline(line[4:]),styles['h2'])]);i+=1;continue
    if line.startswith('## '):story.extend([CondPageBreak(75),Paragraph(inline(line[3:]),styles['h1'])]);i+=1;continue
    m=re.match(r'!\[(.*?)\]\((.*?)\)',line)
    if m:story.append(figure(m.group(1),m.group(2)));figure_count+=1;i+=1;continue
    if line.startswith('```'):
        code=[];i+=1
        while i<len(lines) and not lines[i].strip().startswith('```'):code.append(lines[i]);i+=1
        wrapped=[]
        for row in code:wrapped.extend(textwrap.wrap(row,width=100,replace_whitespace=False,drop_whitespace=False) or [''])
        story.append(Preformatted('\n'.join(wrapped),styles['code']));i+=1;continue
    if line.startswith('|'):
        rows=[]
        while i<len(lines) and lines[i].strip().startswith('|'):
            cells=[v.strip() for v in lines[i].strip().strip('|').split('|')]
            if not all(re.fullmatch(r':?-+:?',v.replace(' ','')) for v in cells):rows.append(cells)
            i+=1
        story.extend(table(rows));table_count+=1;continue
    if re.match(r'^(\d+\.|-) ',line):
        marker,content=line.split(' ',1);story.append(Paragraph(inline(content),styles['bullet'],bulletText='•' if marker=='-' else marker));i+=1;continue
    para=[line];i+=1
    while i<len(lines) and lines[i].strip() and not re.match(r'^(#{2,3} |\||```|!\[|<!--|\d+\. |[-] )',lines[i].strip()):para.append(lines[i].strip());i+=1
    text=' '.join(para);sty=styles['ref'] if re.match(r'^\[\d+\]',text) else styles['body'];story.append(Paragraph(inline(text),sty))
output=ROOT/'technical-report.pdf';doc=ReportDoc(str(output),pagesize=A4,rightMargin=R,leftMargin=L,topMargin=TOP,bottomMargin=BOTTOM,title='Smart Plant Monitoring System',author='; '.join(AUTHORS),subject='BCA152 Microcontrollers — B186: technical report',pageCompression=1)
frame=Frame(L,BOTTOM,TEXT,HEIGHT-TOP-BOTTOM,leftPadding=0,rightPadding=0,topPadding=0,bottomPadding=0)
doc.addPageTemplates([PageTemplate(id='report',frames=[frame],onPage=lambda c,d: cover(c,d) if d.page==1 else decor(c,d))]);doc.multiBuild(story)
print('Built',output,'Figures',figure_count,'Tables',table_count)

