"""Release safety gates must stop before any package or remote mutation."""
import importlib.util
import os
from pathlib import Path
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('publisher',Path(__file__).parents[1]/'scripts/publish_release.py')
publisher=importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)

class ReleaseSafety(unittest.TestCase):
    def stopped(self, answers, message, environment=None):
        env=dict(SPARAMVIEW_VERSION='1.2.4a',GITHUB_SHA='a'*40,GITHUB_REPOSITORY='owner/repo',
                 GITHUB_REF='refs/heads/main',GITHUB_EVENT_NAME='push',GITHUB_RUN_ID='42')
        env.update(environment or {})
        with patch.dict(os.environ,env), patch.object(publisher.subprocess,'check_output',return_value='a'*40), \
             patch.object(publisher,'api',side_effect=answers) as api:
            with self.assertRaisesRegex(RuntimeError,message):publisher.main()
            for call in api.call_args_list:
                self.assertEqual(len(call.args),1,'A failed gate attempted a remote write')
    def test_foreign_tag(self):
        self.stopped([None,{'object':{'type':'commit','sha':'b'*40}}],'another object')
    def test_published_release_immutable(self):
        self.stopped([{'draft':False},{'object':{'type':'commit','sha':'a'*40}}],'already exists')
    def test_unpinned_draft(self):
        self.stopped([{'draft':True},None],'no pinned tag')
    def test_skipped_gate_not_pass(self):
        self.stopped([None,None,{'jobs':[]}],'did not succeed')
    def test_pr_never_publishes(self):
        self.stopped([],'main push',{'GITHUB_EVENT_NAME':'pull_request'})
    def test_provenance(self):
        publisher.check_provenance({'version':'1.2.4a','commit':'abc'},'1.2.4a','abc')
        for data in [{'version':'1.2.3','commit':'abc'},{'version':'1.2.4a','commit':'def'}]:
            with self.assertRaises(RuntimeError):publisher.check_provenance(data,'1.2.4a','abc')


    def test_legacy_release_retirement_requires_verified_new_assets(self):
        endpoint = 'repos/july-rust/SParamView-Public'
        replacement = {'draft': True, 'prerelease': False,
                       'tag_name': 'v1.2.4a', 'target_commitish': 'a'*40,
                       'assets': [{'name': 'sample.zip'}]}
        with patch.object(publisher, 'api', side_effect=[
                replacement, {'object': {'sha': 'a'*40}}]), \
             patch.object(publisher, 'gh') as gh:
            with self.assertRaisesRegex(RuntimeError, 'not fully verified'):
                publisher.retire_v124_after_v124a_published(
                    endpoint, 'july-rust/SParamView-Public',
                    '1.2.4a', 'a'*40, {'sample.zip'})
            gh.assert_not_called()

    def test_legacy_release_retirement_requires_pinned_old_tag(self):
        endpoint = 'repos/july-rust/SParamView-Public'
        replacement = {'draft': False, 'prerelease': False,
                       'tag_name': 'v1.2.4a', 'target_commitish': 'a'*40,
                       'assets': [{'name': 'sample.zip'}]}
        old = {'id': 411355784, 'tag_name': 'v1.2.4'}
        old_ref = {'object': {'sha': 'b'*40, 'type': 'commit'}}
        with patch.object(publisher, 'api', side_effect=[
                replacement, {'object': {'sha': 'a'*40}}, old, old_ref]), \
             patch.object(publisher, 'gh') as gh:
            with self.assertRaisesRegex(RuntimeError, 'tag that moved'):
                publisher.retire_v124_after_v124a_published(
                    endpoint, 'july-rust/SParamView-Public',
                    '1.2.4a', 'a'*40, {'sample.zip'})
            gh.assert_not_called()

    def test_legacy_retirement_deletes_release_then_tag(self):
        endpoint = 'repos/july-rust/SParamView-Public'
        replacement = {'draft': False, 'prerelease': False,
                       'tag_name': 'v1.2.4a', 'target_commitish': 'a'*40,
                       'assets': [{'name': 'sample.zip'}]}
        old = {'id': 411355784, 'tag_name': 'v1.2.4'}
        old_ref = {'object': {'type': 'commit',
                             'sha': '92fe88c83fd1171c8171b04660c95bfebc723264'}}
        with patch.object(publisher, 'api', side_effect=[
                replacement, {'object': {'sha': 'a'*40}},
                old, old_ref, None, None]), \
             patch.object(publisher, 'gh') as gh:
            publisher.retire_v124_after_v124a_published(
                endpoint, 'july-rust/SParamView-Public',
                '1.2.4a', 'a'*40, {'sample.zip'})
            self.assertEqual(gh.call_count, 2)
            self.assertEqual(gh.call_args_list[0].args,
                             ('api', '--method', 'DELETE', endpoint+'/releases/411355784'))
            self.assertEqual(gh.call_args_list[1].args,
                             ('api', '--method', 'DELETE', endpoint+'/git/refs/tags/v1.2.4'))


    def test_existing_v124a_is_preserved_for_ci_only_changes(self):
        endpoint = 'repos/july-rust/SParamView-Public'
        old = '139b05fb112aec3aba4637a32eec48d1cc796672'
        existing = dict(id=411430037,tag_name='v1.2.4a',draft=False,
                        target_commitish=old)
        ref = {'object': {'type':'commit','sha':old}}
        changed = {'status':'ahead','files': [
            {'status':'modified','filename':'tests/open_tests.cpp'},
            {'status':'modified','filename':'.github/workflows/release-3platform.yml'}]}
        with patch.object(publisher,'api',return_value=changed) as api:
            self.assertTrue(publisher.validated_ci_only_v124a(
                endpoint,'july-rust/SParamView-Public','1.2.4a',
                existing,ref,'b'*40))
            self.assertEqual(api.call_count,1)

    def test_existing_v124a_allows_repository_only_guide_updates(self):
        endpoint = 'repos/july-rust/SParamView-Public'
        old = '139b05fb112aec3aba4637a32eec48d1cc796672'
        existing = dict(id=411430037,tag_name='v1.2.4a',draft=False,
                        target_commitish=old)
        ref = {'object': {'type':'commit','sha':old}}
        updated = {'status':'ahead','files': [
            {'status':'modified','filename':'docs/USER_GUIDE_KO.md'},
            {'status':'modified','filename':'docs/USER_GUIDE_EN.md'},
            {'status':'modified','filename':'docs/Revision_History.md'},
            {'status':'modified','filename':'docs/Release_1.2.4a.md'},
            {'status':'modified','filename':'README.md'}]}
        with patch.object(publisher,'api',return_value=updated):
            self.assertTrue(publisher.validated_ci_only_v124a(
                endpoint,'july-rust/SParamView-Public','1.2.4a',
                existing,ref,'b'*40))
        updated['files'].append({'status':'modified','filename':'README_FIRST.txt'})
        with patch.object(publisher,'api',return_value=updated):
            self.assertFalse(publisher.validated_ci_only_v124a(
                endpoint,'july-rust/SParamView-Public','1.2.4a',
                existing,ref,'b'*40))

    def test_existing_v124a_rejects_binary_changes_and_moved_tag(self):
        endpoint = 'repos/july-rust/SParamView-Public'
        old = '139b05fb112aec3aba4637a32eec48d1cc796672'
        existing = dict(id=411430037,tag_name='v1.2.4a',draft=False,
                        target_commitish=old)
        ref = {'object': {'type':'commit','sha':old}}
        changed = {'status':'ahead','files': [
            {'status':'modified','filename':'src/tdr.cpp'}]}
        with patch.object(publisher,'api',return_value=changed):
            self.assertFalse(publisher.validated_ci_only_v124a(
                endpoint,'july-rust/SParamView-Public','1.2.4a',
                existing,ref,'b'*40))
        with patch.object(publisher,'api') as api:
            self.assertFalse(publisher.validated_ci_only_v124a(
                endpoint,'july-rust/SParamView-Public','1.2.4a',
                existing,{'object':{'type':'commit','sha':'c'*40}},'b'*40))
            api.assert_not_called()


    def test_published_ci_only_main_succeeds_without_release_mutation(self):
        old = '139b05fb112aec3aba4637a32eec48d1cc796672'
        new = 'b'*40
        repo = 'july-rust/SParamView-Public'
        env = dict(SPARAMVIEW_VERSION='1.2.4a',GITHUB_SHA=new,
                   GITHUB_REPOSITORY=repo,GITHUB_REF='refs/heads/main',
                   GITHUB_EVENT_NAME='push',GITHUB_RUN_ID='42')
        release = dict(id=411430037,tag_name='v1.2.4a',
                       draft=False,target_commitish=old)
        ref = {'object': {'type':'commit','sha':old}}
        comparison = {'status':'ahead','files': [
            {'status':'modified','filename':'tests/open_tests.cpp'}]}
        jobs = {'jobs': [dict(name=name,conclusion='success',html_url='https://example.invalid')
                         for name in ['Version and release consistency','Core quality gates',
                                      'Windows x64','Windows ARM64',
                                      'Native Windows ARM64 packaged-binary gate',
                                      'macOS Apple Silicon arm64']]}
        with patch.dict(os.environ,env), \
             patch.object(publisher.subprocess,'check_output',return_value=new), \
             patch.object(publisher,'api',side_effect=[release,ref,comparison,jobs]) as api, \
             patch.object(publisher,'gh') as gh:
            publisher.main()
            self.assertEqual(api.call_count,4)
            for call in api.call_args_list:
                self.assertEqual(len(call.args),1)
            gh.assert_not_called()

if __name__=='__main__':unittest.main()
