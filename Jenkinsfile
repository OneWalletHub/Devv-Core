node {
  agent {
    dockerfile {
      dir 'docker'
      filename 'Dockerfile-x86_64-ubuntu16.04-dev'
      label 'my-test-build'
    }
  }
  stage('Checkout') {
    checkout scm
  }
  stage('Build') {
    echo 'Hello from Jenkins!'
    sh 'cat /etc/lsb-release'
    sh 'gcc --version'
  }
  stage('Test') {
      //
  }
  stage('Deploy') {
      //
  }
}
