import apport.packaging
import re


def add_info(report):
    if not 'Stacktrace' in report:
        return

    m = re.search(r'/usr/lib/([\w-]+/)?cinnamon-settings-daemon-3.0',
                  report['Stacktrace'])
    if not m:
        return
    package = apport.packaging.get_file_package(m.group(0))
    report.add_package_info(package)

    # the issue is not in the cinnamon-settings-daemon code so reassign
    for word in report['Stacktrace'].split():
        # update the title to reflect the component and tab
        m = re.search(r'lib([\w-]+)\.so', word)
        if not m:
            continue
        component = m.group(1)
        report['Title'] = '[%s]: %s' % \
            (component, report.get('Title', report.standard_title()))
        tags = ('%s ' % report['Tags']) if report.get('Tags', '') else ''
        report['Tags'] = tags + component
        break  # Stop on the first .so that's the interesting one
