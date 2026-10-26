"""Compile and run real notification formatting offline; never uses local credentials."""
from pathlib import Path
import argparse, base64, email.policy, email.parser, os, shutil, subprocess, tempfile

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', help='Path to an installed C compiler (GCC compatible).')
parser.add_argument('--output-dir', type=Path, help='Keep generated example HTML/JSON here.')
args=parser.parse_args()
component=ROOT/'components/notifications'
if not (component/'notification_format.c').exists():
    raise SystemExit('FAIL: shared notification formatting and PhST feature is not implemented yet.')
compiler=args.cc or shutil.which('gcc') or shutil.which('cc')
if not compiler and Path('C:/Program Files/CodeBlocks/MinGW/bin/gcc.exe').exists():
    compiler='C:/Program Files/CodeBlocks/MinGW/bin/gcc.exe'
if not compiler:raise SystemExit('An installed GCC-compatible compiler is needed; pass --cc PATH.')
cjson=ROOT/'managed_components/espressif__cjson/cJSON'
if not (cjson/'cJSON.c').exists():raise SystemExit('Managed cJSON source missing; build the project first.')
with tempfile.TemporaryDirectory(prefix='plant-format-tests-') as temp:
    work=Path(temp);artifacts=args.output_dir or work/'examples';artifacts.mkdir(parents=True,exist_ok=True)
    executable=work/('format-tests.exe' if os.name=='nt' else 'format-tests')
    command=[compiler,'-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-Wformat=2','-finput-charset=UTF-8',
        '-include',str(ROOT/'tests/host/include/time_compat.h'),
        '-I'+str(ROOT/'tests/host/include'),'-I'+str(component/'include'),'-I'+str(component),
        '-I'+str(cjson),str(ROOT/'tests/host/test_notification_format.c'),
        str(component/'notification_format.c'),str(cjson/'cJSON.c'),'-o',str(executable)]
    subprocess.run(command,check=True)
    subprocess.run([str(executable),str(artifacts)],check=True)
    # The formatter must not depend on the host computer's selected timezone.
    for timezone in ['UTC0','PST8PDT']:
        result=subprocess.run([str(executable),str(artifacts)],env={**os.environ,'TZ':timezone},
            check=True,capture_output=True,text=True)
        print('Timezone independence:',timezone,':',result.stdout.strip().splitlines()[-1])

    if os.name=='nt':
        # Compile an unchanged copy of the sender beside dummy config, not its private header.
        shutil.copyfile(component/'gmail.c',work/'gmail.c')
        (work/'config.local.h').write_text(
            '#define GMAIL_ADDRESS "demo@example.com"\n'
            '#define GMAIL_RECIPIENT "recipient@example.com"\n'
            '#define GMAIL_APP_PASSWORD "abcdefghijklmnop"\n',encoding='utf-8')
        for header in ['esp_tls.h','esp_crt_bundle.h','esp_log.h','esp_timer.h','connectivity.h',
                       'freertos/FreeRTOS.h','freertos/task.h','mbedtls/base64.h','mbedtls/platform_util.h']:
            target=work/header;target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text('#include "host_transport.h"\n',encoding='utf-8')
        smtp=work/'smtp-tests.exe'
        smtp_command=command[:-2]
        smtp_command[smtp_command.index(str(ROOT/'tests/host/test_notification_format.c'))]=str(ROOT/'tests/host/test_gmail_transport.c')
        smtp_command += ['-I'+str(work),str(work/'gmail.c'),'-lcrypt32','-o',str(smtp)]
        subprocess.run(smtp_command,check=True)
        for mode in ['normal','missing-dht','unknown-clock']:
            capture=artifacts/('smtp-'+mode+'.txt')
            subprocess.run([str(smtp),str(capture),mode],check=True)
            wire=capture.read_bytes()
            assert wire.count(b'\r\n.\r\n')==1,'SMTP DATA must end exactly once'
            raw=wire.split(b'DATA\r\n',1)[1].split(b'\r\n.\r\n',1)[0]
            mail=email.parser.BytesParser(policy=email.policy.default).parsebytes(raw)
            assert mail.get_content_type()=='multipart/related'
            assert mail.get_param('type')=='multipart/alternative','Related MIME must name its root type'
            assert not any(part.defects for part in mail.walk()),'Malformed MIME structure'
            alternative=mail.get_payload(0)
            assert alternative.get_content_type()=='multipart/alternative'
            plain=alternative.get_payload(0);html=alternative.get_payload(1)
            assert plain.get_content_type()=='text/plain' and html.get_content_type()=='text/html'
            text=plain.get_content();markup=html.get_content()
            for body in [text,markup]:
                assert 'Plant-01' in body and 'Confirmed moisture state' in body
                assert 'potentiometer' in body
                assert ('clock not synchronized' if mode=='unknown-clock' else '8:15:54 AM PhST') in body
                assert ('Unavailable' if mode=='missing-dht' else '77.0%') in body
            image=mail.get_payload(1)
            assert image['Content-ID']=='<plant-mark>' and 'cid:plant-mark' in markup
            assert image.get_payload(decode=True)==(component/'assets/plant-mark.png').read_bytes()
            for part in [plain,html,image]:
                lines=part.get_payload().splitlines()
                assert all(len(line)<=76 for line in lines),'Base64 line exceeds MIME limit'
                base64.b64decode(''.join(lines),validate=True)
            (artifacts/('gmail-'+mode+'.eml')).write_bytes(raw)
            print('PASS MIME alternatives, inline image, captured time:',mode)
        subprocess.run([str(smtp),str(artifacts/'unused.txt'),'failed-write'],check=True)
