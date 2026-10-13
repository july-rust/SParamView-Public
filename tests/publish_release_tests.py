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

if __name__=='__main__':unittest.main()
