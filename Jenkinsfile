//
// Jenkinsfile - defines CI pipeline
//
// @copywrite  2018 Devvio Inc
//
def install_prefix = "/mnt/efs/jenkins/install"
def source_prefix = "/mnt/efs/jenkins/source"
node('thor-build') {
    def job_name = "${env.JOB_NAME}"
    def clean_job_name = job_name.replace("%2F",".")
    def workspace_name = "${clean_job_name}-${env.BUILD_ID}"
    def install_dir = "${install_prefix}/${workspace_name}"
    def source_dir = "${source_prefix}/${workspace_name}"
    ws(workspace_name) {
	// Change source dir
	dir(source_dir) {
	    stage('Checkout') {
		checkout scm
		stash name:'scm', includes:'*'
	    }
	    stage('Build') {
		unstash 'scm'
		sh 'cat /etc/lsb-release'
		sh 'mkdir build'
		dir('build') {
		    sh "cmake -DCMAKE_BUILD_TYPE=Debug ../src -DCMAKE_INSTALL_PREFIX='${install_dir}'"
		    sh 'make -j4'
		}
	    }
	    stage('Test') {
		unstash 'scm'
		dir('build') {
		    sh 'make test'
		}
	    }
	    stage('Deploy') {
		//
	    }
	}
    }
}
