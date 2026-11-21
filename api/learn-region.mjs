export default {
  fetch(request) {
    const country = request.headers.get('x-vercel-ip-country');
    const region = country === 'CN' ? 'china' : country ? 'international' : null;
    return Response.json({region}, {headers: {'Cache-Control': 'private, no-store'}});
  },
};
