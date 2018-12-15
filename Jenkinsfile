node {
  //agent {
  //  dockerfile {
  //    dir 'docker'
  //    filename 'Dockerfile-x86_64-ubuntu16.04-dev'
  //    label 'my-test-build'
  //  }
  //}
  stage('Checkout') {
    checkout scm
  }
  stage('Build') {
    echo 'Hello from Jenkins!'
    def dockerfile = 'Dockerfile-x86_64-ubuntu16.04-dev'
    dir('docker') {
      def customImage = docker.build("my-image:${env.BUILD_ID}", "-f ${dockerfile} .")
      customImage.inside {
        sh 'cat /etc/lsb-release'
        sh 'gcc --version'
      }
    }
  }
  stage('Test') {
      //
  }
  stage('Deploy') {
      //
  }
}
