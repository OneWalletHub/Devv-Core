node('thor-build') {
  ws("${env.JOB_NAME}-${env.BUILD_ID") {
    stage('Checkout') {
      checkout scm
      stash name:'scm', includes:'*'
    }
    stage('Build') {
	unstash 'scm'
	echo 'Hello from stage Build'
	sh 'pwd'
	sh 'ls'
	sh 'cat /etc/lsb-release'
	sh 'mkdir build'
	dir('build') {
	  sh 'cmake -DCMAKE_BUILD_TYPE=Debug ../src/ -DCMAKE_INSTALL_PREFIX=/mnt/efs/scratch/devv-core/b-$(date +%s)'
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
