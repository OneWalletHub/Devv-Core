node {
  stage('Checkout') {
    git(
      url: 'https://github.com/DevvioInc/Devv-Core.git',
      credentialsId: 'xpc',
      branch: "${branch}"
    )
  }
  stage('Build') {
    echo 'Hello from Jenkins!'
    def pwd = sh('echo $PWD')
    sh 'ls -l'
  }
  stage('Test') {
      //
  }
  stage('Deploy') {
      //
  }
}
