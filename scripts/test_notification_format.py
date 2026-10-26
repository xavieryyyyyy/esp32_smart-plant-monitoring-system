"""Compile and run real notification formatting offline; never uses local credentials."""
# Use filesystem paths that work even when the project folder contains spaces.
from pathlib import Path
# Import argument parsing, MIME/Base64 inspection, process execution, and temporary-folder helpers.
import argparse, base64, email.policy, email.parser, os, shutil, subprocess, tempfile

# Find the repository by this script's location rather than the current terminal folder.
ROOT=Path(__file__).resolve().parents[1]
# Prepare command-line options for an existing compiler and optional saved example files.
parser=argparse.ArgumentParser(description=__doc__)
# Allow --cc to select a GCC-compatible compiler already installed on this computer.
parser.add_argument('--cc', help='Path to an installed C compiler (GCC compatible).')
# Allow --output-dir to preserve generated examples instead of deleting them with temporary files.
parser.add_argument('--output-dir', type=Path, help='Keep generated example HTML/JSON here.')
# Read the options supplied on this invocation.
args=parser.parse_args()
# Find the real notification source; tests exercise production formatting code rather than a second
# implementation.
component=ROOT/'components/notifications'
# Stop clearly if the shared formatter source is absent.
if not (component/'notification_format.c').exists():
    # Return a failing process status with the reason; do not claim the missing feature passed.
    raise SystemExit('FAIL: shared notification formatting and PhST feature is not implemented yet.')
# Prefer an explicit compiler, then gcc/cc available on PATH; this does not install anything.
compiler=args.cc or shutil.which('gcc') or shutil.which('cc')
# On this Windows setup, also recognize the existing CodeBlocks compiler if PATH did not locate one.
if not compiler and Path('C:/Program Files/CodeBlocks/MinGW/bin/gcc.exe').exists():
    # Select that installed fallback compiler by its full executable path.
    compiler='C:/Program Files/CodeBlocks/MinGW/bin/gcc.exe'
# Explain how to choose a compiler when no usable local compiler was found.
if not compiler:raise SystemExit('An installed GCC-compatible compiler is needed; pass --cc PATH.')
# Use the managed cJSON source already used by the firmware build.
cjson=ROOT/'managed_components/espressif__cjson/cJSON'
# Ask for a normal project build if dependencies have not yet been resolved locally.
if not (cjson/'cJSON.c').exists():raise SystemExit('Managed cJSON source missing; build the project first.')
# Create an isolated temporary build folder and delete it automatically when this block finishes.
with tempfile.TemporaryDirectory(prefix='plant-format-tests-') as temp:
    # Create either the requested example-output directory or a temporary examples directory.
    work=Path(temp);artifacts=args.output_dir or work/'examples';artifacts.mkdir(parents=True,exist_ok=True)
    # Choose the executable filename expected by the host operating system.
    executable=work/('format-tests.exe' if os.name=='nt' else 'format-tests')
    # Compile as C11, enable strict warnings, and make warnings fail this host check.
    command=[compiler,'-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-Wformat=2','-finput-charset=UTF-8',
        # Pre-include the Windows/POSIX time compatibility adapter; firmware does not use this
        # adapter.
        '-include',str(ROOT/'tests/host/include/time_compat.h'),
        # Search host stand-in headers first, then the real public/private formatter headers.
        '-I'+str(ROOT/'tests/host/include'),'-I'+str(component/'include'),'-I'+str(component),
        # Compile the assertion-based test program with the real cJSON include path.
        '-I'+str(cjson),str(ROOT/'tests/host/test_notification_format.c'),
        # Link the real formatter and cJSON sources into this temporary host executable.
        str(component/'notification_format.c'),str(cjson/'cJSON.c'),'-o',str(executable)]
    # Build and stop immediately if the compiler exits unsuccessfully.
    subprocess.run(command,check=True)
    # Run all named formatter cases and write examples; a nonzero status fails this Python runner.
    subprocess.run([str(executable),str(artifacts)],check=True)
    # The formatter must not depend on the host computer's selected timezone.
    # Repeat the same cases under two different host timezones; these are reruns, not extra distinct
    # test cases.
    for timezone in ['UTC0','PST8PDT']:
        # Override only the child process's TZ so the machine's own timezone setting remains
        # unchanged.
        result=subprocess.run([str(executable),str(artifacts)],env={**os.environ,'TZ':timezone},
            check=True,capture_output=True,text=True)
        # Report the repeated run's summary to show that PhST formatting did not depend on host
        # timezone.
        print('Timezone independence:',timezone,':',result.stdout.strip().splitlines()[-1])

    # Run the Windows-specific fake SMTP/Base64 checks only on Windows.
    if os.name=='nt':
        # Compile an unchanged copy of the sender beside dummy config, not its private header.
        # Copy the real Gmail sender beside test configuration so its quoted include never reads
        # production credentials.
        shutil.copyfile(component/'gmail.c',work/'gmail.c')
        # Write only dummy account/password definitions inside the temporary test folder.
        (work/'config.local.h').write_text(
            '#define GMAIL_ADDRESS "demo@example.com"\n'
            '#define GMAIL_RECIPIENT "recipient@example.com"\n'
            '#define GMAIL_APP_PASSWORD "abcdefghijklmnop"\n',encoding='utf-8')
        # Provide stand-ins for the ESP-IDF/FreeRTOS/mbedTLS headers needed by the real sender.
        for header in ['esp_tls.h','esp_crt_bundle.h','esp_log.h','esp_timer.h','connectivity.h',
                       'freertos/FreeRTOS.h','freertos/task.h','mbedtls/base64.h','mbedtls/platform_util.h']:
            # Create matching nested header directories inside the temporary folder.
            target=work/header;target.parent.mkdir(parents=True,exist_ok=True)
            # Point each stub header to the shared host_transport declarations.
            target.write_text('#include "host_transport.h"\n',encoding='utf-8')
        # Name the separate executable that exercises the real SMTP sender against fake I/O.
        smtp=work/'smtp-tests.exe'
        # Reuse compile settings but remove the old output-filename pair.
        smtp_command=command[:-2]
        # Replace the formatter test entry point with the fake-SMTP test entry point.
        smtp_command[smtp_command.index(str(ROOT/'tests/host/test_notification_format.c'))]=str(ROOT/'tests/host/test_gmail_transport.c')
        # Add the copied sender and Windows Base64 library, then choose the SMTP-test executable
        # output.
        smtp_command += ['-I'+str(work),str(work/'gmail.c'),'-lcrypt32','-o',str(smtp)]
        # Compile this offline SMTP test; no service is contacted by the fake transport.
        subprocess.run(smtp_command,check=True)
        # Exercise normal readings, failed DHT data, and unknown captured time as three separate
        # SMTP sessions.
        for mode in ['normal','missing-dht','unknown-clock']:
            # Choose a local capture file for this mode's fake SMTP conversation.
            capture=artifacts/('smtp-'+mode+'.txt')
            # Run the real sender against fake replies/writes and require its checks to pass.
            subprocess.run([str(smtp),str(capture),mode],check=True)
            # Read the captured bytes exactly, including CRLF line endings.
            wire=capture.read_bytes()
            # Require one SMTP DATA terminator for the whole multipart message, not one terminator
            # per MIME part.
            assert wire.count(b'\r\n.\r\n')==1,'SMTP DATA must end exactly once'
            # Extract only the transmitted email from between DATA and its final dot; exclude
            # login/SMTP commands.
            raw=wire.split(b'DATA\r\n',1)[1].split(b'\r\n.\r\n',1)[0]
            # Use Python's independent MIME parser to inspect what the actual C sender produced.
            mail=email.parser.BytesParser(policy=email.policy.default).parsebytes(raw)
            # Require the outer container for HTML and its related inline image.
            assert mail.get_content_type()=='multipart/related'
            # Require the related container to identify its multipart/alternative root correctly.
            assert mail.get_param('type')=='multipart/alternative','Related MIME must name its root type'
            # Reject MIME parsing defects anywhere in the nested message.
            assert not any(part.defects for part in mail.walk()),'Malformed MIME structure'
            # Select the first/root MIME child, which holds the plain/HTML alternatives.
            alternative=mail.get_payload(0)
            # Require the alternatives container before inspecting its children.
            assert alternative.get_content_type()=='multipart/alternative'
            # Select plain text first and HTML second, as written by smtp_send_body().
            plain=alternative.get_payload(0);html=alternative.get_payload(1)
            # Verify that both alternatives declare their correct content types.
            assert plain.get_content_type()=='text/plain' and html.get_content_type()=='text/html'
            # Decode the MIME transfer encoding to inspect the real displayed content.
            text=plain.get_content();markup=html.get_content()
            # Apply the same essential-data checks to both the plain and rich email versions.
            for body in [text,markup]:
                # Require the plant identifier and confirmed-state reason in each alternative.
                assert 'Plant-01' in body and 'Confirmed moisture state' in body
                # Require the qualifier that moisture comes from a potentiometer simulation.
                assert 'potentiometer' in body
                # Require the original captured PhST time, or the explicit missing-clock explanation
                # for that mode.
                assert ('clock not synchronized' if mode=='unknown-clock' else '8:15:54 AM PhST') in body
                # Require DHT values when valid, and Unavailable when this event's DHT read failed.
                assert ('Unavailable' if mode=='missing-dht' else '77.0%') in body
            # Select the inline image, which is the next child of the outer related container.
            image=mail.get_payload(1)
            # Require the HTML CID reference and image Content-ID to name the same embedded image.
            assert image['Content-ID']=='<plant-mark>' and 'cid:plant-mark' in markup
            # Decode the image and require an exact byte match with the project's PNG asset.
            assert image.get_payload(decode=True)==(component/'assets/plant-mark.png').read_bytes()
            # Check the Base64 representation of each alternative and the image.
            for part in [plain,html,image]:
                # Read the encoded payload lines before MIME decoding.
                lines=part.get_payload().splitlines()
                # Require at most 76 characters per Base64 line, as the sender promises.
                assert all(len(line)<=76 for line in lines),'Base64 line exceeds MIME limit'
                # Reject invalid Base64 characters/padding instead of silently accepting malformed
                # encoding.
                base64.b64decode(''.join(lines),validate=True)
            # Save the extracted email separately for optional local visual inspection.
            (artifacts/('gmail-'+mode+'.eml')).write_bytes(raw)
            # Print a pass record for this complete MIME/time/image session.
            print('PASS MIME alternatives, inline image, captured time:',mode)
        # Inject a failed write and require the real sender to return failure and release its fake
        # connection.
        subprocess.run([str(smtp),str(artifacts/'unused.txt'),'failed-write'],check=True)
